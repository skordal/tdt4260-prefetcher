// DCPT class

#ifndef DCPT_HH
#define DCPT_HH

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
		void collectPrefetchCandidates(int start, Addr ** prefetch, int & size);
		int getPreviousDelta() const;

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

#endif

