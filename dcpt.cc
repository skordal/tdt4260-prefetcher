// DCPT class

#include <cstring>
#include "dcpt.hh"

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

