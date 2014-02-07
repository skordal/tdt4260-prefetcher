// DCPT class

#ifndef DCPT_HH
#define DCPT_HH

#include "interface.hh"

class DCPTEntry
{
	public:
		static const int NUMBER_OF_DELTAS = 16;

		DCPTEntry(Addr pc);
		Addr getPC() const { return pc; }
	private:
		int getPreviousDelta() const;

		Addr pc, lastAddress, lastPrefetch;
		int deltaArray[DCPTEntry::NUMBER_OF_DELTAS];
		int deltaNext;
};

#endif

