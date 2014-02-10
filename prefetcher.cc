/*
 * A sample prefetcher which does sequential one-block lookahead.
 * This means that the prefetcher fetches the next block _after_ the one that
 * was just accessed. It also ignores requests to blocks already in the cache.
 */

#include "interface.hh"
#include <list>

class DCPTEntry
{
	public:
		static const int NUMBER_OF_DELTAS = 16;

		DCPTEntry(Addr pc);
		Addr getPC() const { return pc; }

		void miss(Addr & addr, Addr ** prefetch, int & size);
	private:
		void collectPrefetchCandidates(int start, int stop, Addr ** prefetch, int & size) const;

		Addr pc, lastAddress, lastPrefetch;
		int deltaArray[DCPTEntry::NUMBER_OF_DELTAS];
		int deltaNext;
};

class DCPTTable
{
	public:
		DCPTTable(int entries);
		~DCPTTable();

		DCPTEntry * getEntry(Addr pc);
	private:
		std::list<DCPTEntry *> table;
		int entries;
};

static DCPTTable * table;

#define DELTAPTR_INC(x)	((x + NUMBER_OF_DELTAS + 1) % NUMBER_OF_DELTAS)
#define DELTAPTR_DEC(x)	((x + NUMBER_OF_DELTAS - 1) % NUMBER_OF_DELTAS)

using namespace std;

DCPTEntry::DCPTEntry(Addr pc) : pc(pc), lastAddress(0), lastPrefetch(0), deltaNext(0)
{
	memset(deltaArray, 0, NUMBER_OF_DELTAS * sizeof(int));
}

void DCPTEntry::miss(Addr & addr, Addr ** prefetch, int & size)
{
	int delta = addr - lastAddress;
	delta %= (1 << 10);

	if(delta == 0)
		return;

	deltaArray[deltaNext] = delta;

	int start = DELTAPTR_DEC(deltaNext);
	deltaNext = DELTAPTR_INC(deltaNext);

	int a = deltaArray[start], b = delta;

	*prefetch = 0;
	size = 0;

	for(int i = start; i != DELTAPTR_DEC(deltaNext); i = DELTAPTR_DEC(i))
	{
		if(deltaArray[DELTAPTR_DEC(i)] == a && deltaArray[i] == b)
		{
			collectPrefetchCandidates(DELTAPTR_INC(i), start, prefetch, size);
			break;
		}
	}
}

void DCPTEntry::collectPrefetchCandidates(int start, int stop, Addr ** prefetch, int & size) const
{
	list<Addr> candidates;
	list<Addr>::iterator it;
	int prevAddress = lastAddress;
	int a = 0;

	for(int i = start; i != stop; i = DELTAPTR_INC(i))
	{
		if(!in_cache(prevAddress + deltaArray[i]))
			candidates.push_front(prevAddress + deltaArray[i]);
		prevAddress += deltaArray[i];
	}

	for(it = candidates.begin(); it != candidates.end(); ++it)
	{
		Addr addr = *it;
		if(addr == lastPrefetch)
			it = candidates.erase(it, candidates.end());
	}

	*prefetch = new Addr[candidates.size()];
	for(it = candidates.begin(); it != candidates.end(); ++it)
		(*prefetch)[a++] = *it;
	size = candidates.size();
}

DCPTTable::DCPTTable(int entries) : entries(entries)
{

}

DCPTEntry * DCPTTable::getEntry(Addr pc)
{
	list<DCPTEntry *>::iterator i = table.begin();
	for(; i != table.end(); ++i)
	{
		DCPTEntry * entry = *i;
		if(pc == entry->getPC())
		{
			i = table.erase(i);
			table.push_front(entry);
			return entry;
		}
	}

	DCPTEntry * newEntry = new DCPTEntry(pc);
	table.push_front(newEntry);

	if(table.size() > entries)
	{
		DCPTEntry * last = table.back();
		delete last;
		table.pop_back();
	}

	return newEntry;
}

void prefetch_init(void)
{
	/* Called before any calls to prefetch_access. */
	/* This is the place to initialize data structures. */

	table = new DCPTTable(80);
	DPRINTF(HWPrefetch, "Initialized DCPT-based prefetcher\n");
}

void prefetch_access(AccessStat stat)
{
	int size = 0;
	Addr * prefetchList = 0;

	if(stat.miss)
	{
		DCPTEntry * entry = table->getEntry(stat.pc);
		entry->miss(stat.mem_addr, &prefetchList, size);
	}

	if(prefetchList != 0)
	{
		if(current_queue_size() >= MAX_QUEUE_SIZE - size)
		{
			delete[] prefetchList;
		} else {
			for(int i = 0; i < size; ++i)
				issue_prefetch(prefetchList[i]);
			delete[] prefetchList;
		}
	} else {
		if(stat.miss)
			issue_prefetch(stat.mem_addr + BLOCK_SIZE);
	}
}

void prefetch_complete(Addr addr)
{
}

