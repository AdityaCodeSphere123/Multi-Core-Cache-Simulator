#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include "main.hpp"
#include "bus.hpp"

using namespace std;

// External references
extern vector<int> pendingOperations;
extern vector<int> readCount;
extern vector<int> writeCount;
extern vector<int> missCount;
extern vector<int> evictionCount;
extern vector<int> writebackCount;
extern vector<long long> trafficBytes;

int operationCounter = 0;

int convertHexToInt(const string &hexString)
{
    string hexDigits = hexString;
    if (hexDigits.length() >= 2 && (hexDigits.substr(0, 2) == "0x" || hexDigits.substr(0, 2) == "0X"))
    {
        hexDigits = hexDigits.substr(2);
    }
    int addressValue = stoul(hexDigits, nullptr, 16);
    stringstream formatter;
    formatter << "0x" << setfill('0') << setw(8) << hex << addressValue;
    string paddedResult = formatter.str();

    return addressValue;
}

int processReadMiss(int processorId, int setIndex, int tagValue, bool &triggeredWriteback)
{
    CacheUnit &targetCache = processorCaches[processorId];
    int selectedWay = -1;

    // Search for invalid line first
    int wayIdx = 0;
    while (wayIdx < associativity)
    {
        if (coherenceTable[processorId][setIndex][wayIdx] == CoherenceState::INVALID)
        {
            selectedWay = wayIdx;
            break;
        }
        wayIdx++;
    }

    // If all valid, evict LRU block
    if (selectedWay == -1)
    {
        selectedWay = targetCache.lruOrder[setIndex].front();
        targetCache.lruOrder[setIndex].erase(targetCache.lruOrder[setIndex].begin());
        evictionCount[processorId]++;

        if (targetCache.dirtyFlags[setIndex][selectedWay])
        {
            processorCaches[processorId].isStalled = true;
            int evictedTag = targetCache.tagArray[setIndex][selectedWay];
            int evictedAddr = (evictedTag << (numSetBits + numBlockBits)) | (setIndex << numBlockBits);
            dataTransferQueue.push_back(BusDataTransfer{evictedAddr, processorId, false, true, false, 100});
            triggeredWriteback = true;
        }
    }
    else
    {
        // Remove from LRU if present
        auto foundIt = find(targetCache.lruOrder[setIndex].begin(), targetCache.lruOrder[setIndex].end(), selectedWay);
        if (foundIt != targetCache.lruOrder[setIndex].end())
        {
            targetCache.lruOrder[setIndex].erase(foundIt);
        }
    }

    // Update cache metadata
    targetCache.tagArray[setIndex][selectedWay] = tagValue;
    targetCache.dirtyFlags[setIndex][selectedWay] = false;
    targetCache.lruOrder[setIndex].push_back(selectedWay);
    return selectedWay;
}

int processWriteMiss(int processorId, int setIndex, int tagValue, bool &triggeredWriteback)
{
    CacheUnit &targetCache = processorCaches[processorId];
    int selectedWay = -1;

    // Search for invalid line
    int wayIdx = 0;
    while (wayIdx < associativity)
    {
        if (coherenceTable[processorId][setIndex][wayIdx] == CoherenceState::INVALID)
        {
            selectedWay = wayIdx;
            break;
        }
        wayIdx++;
    }

    // Evict LRU if necessary
    if (selectedWay == -1)
    {
        selectedWay = targetCache.lruOrder[setIndex].front();
        targetCache.lruOrder[setIndex].erase(targetCache.lruOrder[setIndex].begin());
        evictionCount[processorId]++;
        
        if (targetCache.dirtyFlags[setIndex][selectedWay])
        {
            int evictedTag = targetCache.tagArray[setIndex][selectedWay];
            int evictedAddr = (evictedTag << (numSetBits + numBlockBits)) | (setIndex << numBlockBits);
            dataTransferQueue.push_back(BusDataTransfer{evictedAddr, processorId, false, true, false, 100});
            triggeredWriteback = true;
        }
    }
    else
    {
        auto foundIt = find(targetCache.lruOrder[setIndex].begin(), targetCache.lruOrder[setIndex].end(), selectedWay);
        if (foundIt != targetCache.lruOrder[setIndex].end())
        {
            targetCache.lruOrder[setIndex].erase(foundIt);
        }
    }

    // Update metadata for write
    targetCache.tagArray[setIndex][selectedWay] = tagValue;
    targetCache.dirtyFlags[setIndex][selectedWay] = true;
    targetCache.lruOrder[setIndex].push_back(selectedWay);
    return selectedWay;
}

void executeMemoryOperation(pair<char, const char *> traceEntry, int processorId)
{
    operationCounter++;
    
    char opType = traceEntry.first;
    string addressStr = traceEntry.second;

    int memAddr = convertHexToInt(addressStr);
    
    // Skip if pending operation exists
    if (pendingOperations[processorId] != -1)
    {
        totalCycles[processorId]++;
        return;
    }

    // Extract cache indexing fields
    int setIndex = (memAddr >> numBlockBits) & ((1 << numSetBits) - 1);
    int tagBits = memAddr >> (numSetBits + numBlockBits);
    
    bool foundMatch = false;
    int matchedWay = -1;

    CacheUnit &currentCache = processorCaches[processorId];

    if (opType == 'R')
    {
        // Search for tag match
        int searchIdx = 0;
        while (searchIdx < associativity)
        {
            if (coherenceTable[processorId][setIndex][searchIdx] != CoherenceState::INVALID && 
                currentCache.tagArray[setIndex][searchIdx] == tagBits)
            {
                foundMatch = true;
                matchedWay = searchIdx;
                break;
            }
            searchIdx++;
        }

        if (foundMatch)
        {
            // Update LRU on hit
            auto lruIt = find(currentCache.lruOrder[setIndex].begin(), currentCache.lruOrder[setIndex].end(), matchedWay);
            if (lruIt != currentCache.lruOrder[setIndex].end())
            {
                currentCache.lruOrder[setIndex].erase(lruIt);
            }
            currentCache.lruOrder[setIndex].push_back(matchedWay);
        }
        else
        {
            // Read miss - initiate bus read
            pendingRequests.push_back(BusTransaction{processorId, memAddr, BusRequestType::READ_SHARED});
            processorCaches[processorId].isStalled = true;
        }
    }
    else
    {
        // Write operation - search for existing block
        int searchIdx = 0;
        while (searchIdx < associativity)
        {
            if (coherenceTable[processorId][setIndex][searchIdx] != CoherenceState::INVALID && 
                currentCache.tagArray[setIndex][searchIdx] == tagBits)
            {
                foundMatch = true;
                matchedWay = searchIdx;
                break;
            }
            searchIdx++;
        }

        if (foundMatch)
        {
            CoherenceState currentState = coherenceTable[processorId][setIndex][matchedWay];
            
            if (currentState == CoherenceState::EXCLUSIVE || currentState == CoherenceState::MODIFIED)
            {
                // Can write locally
                auto lruIt = find(currentCache.lruOrder[setIndex].begin(), currentCache.lruOrder[setIndex].end(), matchedWay);
                if (lruIt != currentCache.lruOrder[setIndex].end())
                {
                    currentCache.lruOrder[setIndex].erase(lruIt);
                }
                currentCache.lruOrder[setIndex].push_back(matchedWay);
                currentCache.dirtyFlags[setIndex][matchedWay] = true;
                
                if (currentState == CoherenceState::EXCLUSIVE)
                {
                    coherenceTable[processorId][setIndex][matchedWay] = CoherenceState::MODIFIED;
                }
            }
            else
            {
                // Shared state - need upgrade
                pendingRequests.push_back(BusTransaction{processorId, memAddr, BusRequestType::UPGRADE_REQUEST});
                auto lruIt = find(currentCache.lruOrder[setIndex].begin(), currentCache.lruOrder[setIndex].end(), matchedWay);
                if (lruIt != currentCache.lruOrder[setIndex].end())
                {
                    currentCache.lruOrder[setIndex].erase(lruIt);
                }
                currentCache.lruOrder[setIndex].push_back(matchedWay);
            }
        }
        else
        {
            // Write miss
            pendingRequests.push_back(BusTransaction{processorId, memAddr, BusRequestType::READ_EXCLUSIVE});
            processorCaches[processorId].isStalled = true;
        }
    }
}
