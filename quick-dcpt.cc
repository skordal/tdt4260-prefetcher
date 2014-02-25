/* DCPT implementation take 2 */

#include <cstdlib>
#include <cmath>
#include <cstring>
#include <list>

#include "interface.hh"

#define ALGOSWITCH_NUM	800000UL

using namespace std;

/* Notes:
 * Why divide BLOCK_SIZE by 2? According to the DCPT-P paper:
 * "By using deltas that are smaller than a cache block we enable
 * DCPT-P to start prefetching without waiting for too many misses
 * to the L2".
 */

/* ---------- Modular Integer ---------- */

template<int modulo>
class modint
{
	public:
		modint(int init = 0) { value = init % modulo; }

		modint & operator=(int value) { this->value = value % modulo; return *this; }

		modint operator+(int add) const { return modint<modulo>(value + add); }
		modint operator-(int sub) const { return modint<modulo>((value + modulo - (sub % modulo))); }
		modint & operator++() { value = (value + modulo + 1) % modulo; return *this; }
		modint & operator--() { value = (value + modulo - 1) % modulo; return *this; }
		modint operator++(int) { int retval = value; value = (value + modulo + 1) % modulo; return modint<modulo>(retval); }
		modint operator--(int) { int retval = value; value = (value + modulo - 1) % modulo; return modint<modulo>(retval); }

		bool operator==(int value) const { return (value % modulo) == this->value; }
		bool operator==(const modint<modulo> & value) const { return this->value == value.value; }
		bool operator!=(int value) const { return !(*this == value); }
		bool operator!=(const modint<modulo> & value) const { return !(*this == value); }

		operator int() const { return value; }
	private:
		int value;
};

/* ---------- DCPTEntry ---------- */

class DCPTEntry
{
	public:
		static const int NUM_DELTAS = 16;
		static const int NUM_DELTA_BITS = 22;
		static const int NUM_MASKED_BITS = 8;

		DCPTEntry(Addr pc);
		Addr getPC() const { return pc; }

		void miss(Addr address);
	private:
		int minDelta() const { return 1; }
		int maxDelta() const { return -(-(1 << NUM_DELTA_BITS)); }
		int partialMask() const { return -(1 << NUM_MASKED_BITS); }
		int blockSize() const { return BLOCK_SIZE >> 1; }

		void issuePrefetches(int patternStart, int patternEnd);

		Addr pc, lastAddress, lastPrefetch;
		modint<NUM_DELTAS> deltaPtr;
		int deltas[NUM_DELTAS];
};

DCPTEntry::DCPTEntry(Addr pc) : pc(pc), lastAddress(0), lastPrefetch(0), deltaPtr(0)
{
	memset(deltas, 0, sizeof(sizeof(int) * NUM_DELTAS));
}

void DCPTEntry::miss(Addr address)
{
	int delta = (address - lastAddress) / blockSize();
	if(delta > maxDelta())
		delta = maxDelta();
	if(delta == 0)
		delta = minDelta();

	deltas[deltaPtr] = delta;
	int start = deltaPtr - 1;
	++deltaPtr;

	int a = deltas[start], b = delta;
	bool fullMatchFound = false;

	lastAddress = address;

	// Try to find a match by looking at all bits in the deltas:
	for(modint<NUM_DELTAS> i = start; i != deltaPtr; --i)
	{
		if(deltas[i - 1] == a && deltas[i] == b)
		{
			issuePrefetches(i + 1, start);
			fullMatchFound = true;
			break;
		}
	}

	// Do partial matching if no full match was found:
	if(!fullMatchFound)
	{
		a &= partialMask();
		b &= partialMask();
		for(modint<NUM_DELTAS> i = start; i != deltaPtr; --i)
		{
			if((deltas[i - 1] & partialMask()) == a && (deltas[i] & partialMask()) == b)
			{
				issuePrefetches(i + 1, start);
				break;
			}
		}
	}
}

void DCPTEntry::issuePrefetches(int patternStart, int patternEnd)
{
	Addr candidates[NUM_DELTAS];
	Addr prev = lastAddress;
	int cStart = 0, cEnd = 0;

	// Create prefetch candidate list:
	for(modint<NUM_DELTAS> i = patternStart; i != patternEnd; ++i)
	{
		candidates[cEnd++] = prev + deltas[i] * blockSize();
		if(lastPrefetch == candidates[cEnd - 1])
			cStart = cEnd;
		prev = candidates[cEnd - 1];
	}

	// Issue prefetches:
	for(int i = cStart; i < cEnd + 1; ++i)
	{
		if(!in_cache(candidates[i]) && !in_mshr_queue(candidates[i]) && candidates[i] < MAX_PHYS_MEM_ADDR && current_queue_size() < MAX_QUEUE_SIZE)
		{
			issue_prefetch(candidates[i]);
			set_prefetch_bit(candidates[i]);
			lastPrefetch = candidates[i];
		}

	}
}

/* ---------- DCPTTable ---------- */

class DCPTTable
{
	public:
		static const int MAX_ENTRIES = 42;

		DCPTTable() : accuracy(0.5) {}
		DCPTEntry * get(Addr pc, bool create = true);
		double accuracy;
	private:
		list<DCPTEntry *> table;
};

DCPTEntry * DCPTTable::get(Addr pc, bool create)
{
	list<DCPTEntry *>::iterator i;

	for(i = table.begin(); i != table.end(); ++i)
	{
		DCPTEntry * entry = *i;
		if(entry->getPC() == pc)
		{
			i = table.erase(i);
			table.push_front(entry);
			return entry;
		}
	}

	if(!create)
		return 0;

	DCPTEntry * newEntry = new DCPTEntry(pc);
	if(table.size() >= MAX_ENTRIES)
	{
		DCPTEntry * old = table.back();
		table.pop_back();
		delete old;
	}

	table.push_front(newEntry);
	return newEntry;
}

/* ---------- RPTEntry ---------- */

struct RPTEntry
{
		RPTEntry(Addr pc);
		void miss(Addr addr);

		Addr pc, lastAddress;
		int delta;
};

RPTEntry::RPTEntry(Addr pc) : pc(pc), lastAddress(0), delta(0)
{

}

void RPTEntry::miss(Addr addr)
{
	int newDelta = addr - lastAddress;

	if(delta == newDelta && !in_mshr_queue(addr + delta) && !in_cache(addr + delta))
	{
		issue_prefetch(addr + delta);
		set_prefetch_bit(addr + delta);
	}

	delta = newDelta;
	lastAddress = addr;
}

/* ---------- RPTTable ---------- */

class RPTTable
{
	public:
		static const int MAX_ENTRIES = 128;

		RPTTable();
		RPTEntry * get(Addr pc);
		double accuracy;
	private:
		list<RPTEntry *> table;
};

RPTTable::RPTTable() : accuracy(0.5)
{
}

RPTEntry * RPTTable::get(Addr pc)
{
	list<RPTEntry *>::iterator i;

	for(i = table.begin(); i != table.end(); ++i)
	{
		RPTEntry * entry = *i;
		if(entry->pc == pc)
		{
			i = table.erase(i);
			table.push_front(entry);
			return entry;
		}
	}

	RPTEntry * newEntry = new RPTEntry(pc);
	if(table.size() > MAX_ENTRIES)
	{
		RPTEntry * old = table.back();
		table.pop_back();
		delete old;
	}

	table.push_front(newEntry);
	return newEntry;
}

/* ---------- M5 Interface Code ---------- */
static DCPTTable * tableDCPT;
static RPTTable * tableRPT;

static enum { RPT, DCPT } activeAlgorithm = RPT;
static unsigned long successes, hits;

void prefetch_init(void)
{
	tableDCPT = new DCPTTable;
	tableRPT = new RPTTable;

	successes = 0;
	hits = 1;

	DPRINTF(HWPrefetch, "Initialized DCPT-based prefetcher\n");
}

void prefetch_access(AccessStat stat)
{
	if(!stat.miss && get_prefetch_bit(stat.mem_addr))
	{
		++hits;
		++successes;
	} else if(!stat.miss)
		++hits;

	if(activeAlgorithm == RPT)
		tableRPT->get(stat.pc)->miss(stat.mem_addr);
	else
		tableDCPT->get(stat.pc)->miss(stat.mem_addr);

	if(hits >= ALGOSWITCH_NUM)
	{
		successes = 0;
		hits = 1;

		if(activeAlgorithm == RPT)
			tableRPT->accuracy = (double) successes / (double) hits;
		else
			tableDCPT->accuracy = (double) successes / (double) hits;

		int temp = rand() % 100;
		int selectA = int(100.0 * tableDCPT->accuracy);
		int selectB = int(100.0 * tableRPT->accuracy);

		// Favour the algorithm with the highest accuracy:
		if(tableDCPT->accuracy >= tableRPT->accuracy) // (initially favour DCPT, because we start with RPT)
		{
			if(selectA <= temp)
				activeAlgorithm = DCPT;
			else
				activeAlgorithm = RPT;
		} else {
			if(selectB <= temp)
				activeAlgorithm = RPT;
			else
				activeAlgorithm = DCPT;
		}
	}
}

void prefetch_complete(Addr addr)
{
}
