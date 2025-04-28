#ifndef CACHE_HPP
#define CACHE_HPP

#include <vector>
#include <utility>

// Execute a memory operation from trace for specified processor
void executeMemoryOperation(std::pair<char, const char *> traceEntry, int processorId);

// Handle cache read miss - returns way index where data is loaded
int processReadMiss(int processorId, int setIndex, int tagValue, bool &triggeredWriteback);

// Handle cache write miss - returns way index where data is loaded
int processWriteMiss(int processorId, int setIndex, int tagValue, bool &triggeredWriteback);

#endif // CACHE_HPP
