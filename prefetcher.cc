/*
 * A sample prefetcher which does sequential one-block lookahead.
 * This means that the prefetcher fetches the next block _after_ the one that
 * was just accessed. It also ignores requests to blocks already in the cache.
 */

#include "interface.hh"
#include "dcpt.hh"

static DCPTTable * table;

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

