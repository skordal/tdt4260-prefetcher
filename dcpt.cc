// DCPT class

#include <cstring>
#include "dcpt.hh"

DCPTEntry::DCPTEntry(Addr pc) : pc(pc), lastAddress(0), lastPrefetch(0), deltaNext(0)
{
	memset(deltaArray, 0, NUMBER_OF_DELTAS * sizeof(int));
}

int DCPTEntry::getPreviousDelta() const
{
	int index = deltaNext - 1;
	if(index < 0)
		index = index + NUMBER_OF_DELTAS;
	
	return deltaArray[index];
}

