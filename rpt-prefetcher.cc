/*
 * A sample prefetcher which does sequential one-block lookahead.
 * This means that the prefetcher fetches the next block _after_ the one that
 * was just accessed. It also ignores requests to blocks already in the cache.
 */

#include <map>
#include "interface.hh"

struct RPTEntry
{
		RPTEntry(Addr pc);
		Addr miss(Addr addr);

		Addr pc, lastAddress;
		int delta;
		RPTEntry * next, * prev;
};

RPTEntry::RPTEntry(Addr pc) : pc(pc), lastAddress(0), delta(0), next(0), prev(0)
{

}

Addr RPTEntry::miss(Addr addr)
{
	int newDelta = addr - lastAddress;

	if(delta == newDelta)
		issue_prefetch(addr + delta);
	delta = newDelta;
}

class RPTTable
{
	public:
		static const int MAX_ENTRIES = 128;

		RPTTable();
		RPTEntry * get(Addr pc, Addr access);
	private:
		std::map<Addr, RPTEntry *> entryMap;
		RPTEntry * head, * tail;
		int currentEntries;
};

RPTTable::RPTTable() : currentEntries(0), head(0), tail(0)
{
}

RPTEntry * RPTTable::get(Addr pc, Addr access)
{
	if(entryMap.find(pc) == entryMap.end())
	{
		RPTEntry * newEntry = new RPTEntry(pc);
		if(currentEntries == MAX_ENTRIES)
		{
			RPTEntry * oldest = tail;
			tail = oldest->next;
			tail->prev = 0;
			entryMap.erase(oldest->pc);
			delete oldest;
		}

		head->next = newEntry;
		newEntry->prev = head;
		head = newEntry;

		entryMap[pc] = newEntry;

		++currentEntries;
	}

	return entryMap[pc];
}

static RPTTable * table;

void prefetch_init(void)
{
	table = new RPTTable;
}

void prefetch_access(AccessStat stat)
{
	if(stat.miss)
	{
		RPTEntry * entry = table->get(stat.pc);
		entry->miss(stat.mem_addr);
	}
}

void prefetch_complete(Addr addr) {
    /*
     * Called when a block requested by the prefetcher has been loaded.
     */
}
