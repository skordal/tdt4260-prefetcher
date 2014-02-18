/* DCPT implementation take 2 */

#include <cstring>
#include <list>

#include "interface.hh"

using namespace std;

/* Notes:
 * Why divide BLOCK_SIZE by 2? According to the DCPT-P paper:
 * "By using deltas that are smaller than a cache block we enable
 * DCPT-P to start prefetching without waiting for too many misses
 * to the L2".
 */

/* ---------- Modular Integer Stuff ---------- */

template<int modulo>
class modint
{
	public:
		modint(int init = 0) { value = init % modulo; }

		modint & operator=(int value) { this->value = value % modulo; return *this; }

		modint operator+(int add) { return modint<modulo>(value + add); }
		modint operator-(int sub) { return modint<modulo>((value + modulo - (sub % modulo))); }
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

/* ---------- DCPTEntry Stuff ---------- */

class DCPTEntry
{
	public:
		static const int NUM_DELTAS = 16;
		static const int NUM_DELTA_BITS = 16;
		static const int NUM_MASKED_BITS = 8;

		DCPTEntry(Addr pc);

		Addr getPC() const { return pc; }

		void miss(Addr address);
	private:
		int minDelta() const { return 1; }
		int maxDelta() const { return -(-(1 << NUM_DELTA_BITS)); }
		int partialMask() const { return -(1 << NUM_MASKED_BITS); }
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
	int delta = (address - lastAddress) / (BLOCK_SIZE);
	if(delta > maxDelta())
		delta = maxDelta();
	if(delta == 0)
		delta = minDelta();

	deltas[deltaPtr] = delta;
	int start = deltaPtr - 1;
	++deltaPtr;

	int a = deltas[start], b = delta;
	bool fullMatchFound = false;

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
	a &= partialMask();
	b &= partialMask();
	for(modint<NUM_DELTAS> i = start; i != deltaPtr && !fullMatchFound; --i)
	{
		if((deltas[i - 1] & partialMask()) == a && (deltas[i] & partialMask()) == b)
		{
			issuePrefetches(i + 1, start);
			break;
		}
	}

	lastAddress = address;
}

void DCPTEntry::issuePrefetches(int patternStart, int patternEnd)
{
	Addr candidates[NUM_DELTAS];
	Addr prev = lastAddress;
	int cStart = 0, cEnd = 0;

	// Create prefetch candidate list:
	for(modint<NUM_DELTAS> i = patternStart; i != patternEnd; ++i)
	{
		candidates[cEnd++] = prev + deltas[i] * (BLOCK_SIZE);
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

/* ---------- DCPTTable Stuff ---------- */

class DCPTTable
{
	public:
		static const int MAX_ENTRIES = 82;
		DCPTEntry * get(Addr pc, bool create = true);
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

/* ---------- M5 Interface Code ---------- */
static DCPTTable * table;

void prefetch_init(void)
{
	table = new DCPTTable;
	DPRINTF(HWPrefetch, "Initialized DCPT-based prefetcher\n");
}

void prefetch_access(AccessStat stat)
{
	if(stat.miss)
		table->get(stat.pc)->miss(stat.mem_addr);
}

void prefetch_complete(Addr addr)
{
}
