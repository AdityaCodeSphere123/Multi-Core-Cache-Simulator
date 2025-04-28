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
#include "cache.hpp"

vector<int> pendingOperations(4, -1);
bool busOccupied = false;
int busTickCounter = 0;
int debugCounter = 0;

void processBusTransactions()
{
    busTickCounter++;
    
    while (pendingRequests.size())
    {
        BusTransaction currentReq = pendingRequests.front();
        pendingRequests.erase(pendingRequests.begin());

        int requestorCore = currentReq.requestorId;
        int targetAddr = currentReq.memoryAddress;
        BusRequestType requestType = currentReq.reqType;

        int setIndex = (targetAddr >> numBlockBits) & ((1 << numSetBits) - 1);
        int tagBits = targetAddr >> (numSetBits + numBlockBits);

        if (busOccupied)
        {
            processorCaches[requestorCore].isStalled = true;
            stalledCycles[requestorCore]++;
            continue;
        }
        
        pendingOperations[requestorCore] = targetAddr;
        
        if (requestType == BusRequestType::READ_SHARED)
        {
            busOccupied = true;
            busTransactionCount++;
            missCount[requestorCore]++;
            
            bool foundInOther = false;
            int otherCore = 0;
            
            while (otherCore < 4)
            {
                if (otherCore != requestorCore)
                {
                    int wayIdx = 0;
                    while (wayIdx < associativity)
                    {
                        if (processorCaches[otherCore].tagArray[setIndex][wayIdx] == tagBits && 
                            coherenceTable[otherCore][setIndex][wayIdx] != CoherenceState::INVALID)
                        {
                            foundInOther = true;
                            processorCaches[requestorCore].isStalled = true;
                            dataTransferQueue.push_back(BusDataTransfer{targetAddr, requestorCore, false, false, false, 1 << (numBlockBits - 1)});
                            trafficBytes[otherCore] += processorCaches[otherCore].bytesPerBlock;
                            
                            if (coherenceTable[otherCore][setIndex][wayIdx] == CoherenceState::MODIFIED)
                            {
                                coherenceTable[otherCore][setIndex][wayIdx] = CoherenceState::SHARED;
                                processorCaches[otherCore].isStalled = true;
                                dataTransferQueue.push_back(BusDataTransfer{targetAddr, otherCore, false, true, false, 100});
                                if (processorRunning[otherCore])
                                {
                                    totalCycles[otherCore] -= ((1 << (numBlockBits - 1)) + 101);
                                    stalledCycles[otherCore] += (1 << (numBlockBits - 1)) + 1;
                                }
                                pendingOperations[otherCore] = targetAddr;
                            }
                            else if (coherenceTable[otherCore][setIndex][wayIdx] == CoherenceState::EXCLUSIVE)
                            {
                                coherenceTable[otherCore][setIndex][wayIdx] = CoherenceState::SHARED;
                            }
                            break;
                        }
                        wayIdx++;
                    }
                }
                if (foundInOther)
                {
                    break;
                }
                otherCore++;
            }
            
            if (!foundInOther)
            {
                processorCaches[requestorCore].isStalled = true;
                dataTransferQueue.push_back(BusDataTransfer{targetAddr, requestorCore, false, false, false, 100});
            }
        }
        else if (requestType == BusRequestType::READ_EXCLUSIVE)
        {
            busOccupied = true;
            busTransactionCount++;
            missCount[requestorCore]++;
            
            bool foundInOther = false;
            int otherCore = 0;
            
            while (otherCore < 4)
            {
                if (otherCore != requestorCore)
                {
                    int wayIdx = 0;
                    while (wayIdx < associativity)
                    {
                        if (processorCaches[otherCore].tagArray[setIndex][wayIdx] == tagBits && 
                            coherenceTable[otherCore][setIndex][wayIdx] != CoherenceState::INVALID)
                        {
                            foundInOther = true;
                            
                            if (coherenceTable[otherCore][setIndex][wayIdx] == CoherenceState::MODIFIED)
                            {
                                processorCaches[otherCore].isStalled = true;
                                dataTransferQueue.push_back(BusDataTransfer{targetAddr, otherCore, false, true, false, 100});
                                if (processorRunning[otherCore])
                                    totalCycles[otherCore] -= 101;
                                pendingOperations[otherCore] = targetAddr;
                            }
                            coherenceTable[otherCore][setIndex][wayIdx] = CoherenceState::INVALID;
                        }
                        wayIdx++;
                    }
                }
                otherCore++;
            }
            
            processorCaches[requestorCore].isStalled = true;
            if (foundInOther)
                invalidationCount[requestorCore]++;
            dataTransferQueue.push_back(BusDataTransfer{targetAddr, requestorCore, true, false, false, 100});
        }
        else if (requestType == BusRequestType::UPGRADE_REQUEST)
        {
            int targetWay = -1;
            int wayIdx = 0;
            
            while (wayIdx < associativity)
            {
                if (processorCaches[requestorCore].tagArray[setIndex][wayIdx] == tagBits && 
                    coherenceTable[requestorCore][setIndex][wayIdx] == CoherenceState::SHARED)
                {
                    targetWay = wayIdx;
                    break;
                }
                wayIdx++;
            }

            if (targetWay != -1)
            {
                busTransactionCount++;

                // Invalidate in other caches
                int otherCore = 0;
                while (otherCore < 4)
                {
                    if (otherCore != requestorCore)
                    {
                        int searchWay = 0;
                        while (searchWay < associativity)
                        {
                            if (processorCaches[otherCore].tagArray[setIndex][searchWay] == tagBits && 
                                coherenceTable[otherCore][setIndex][searchWay] != CoherenceState::INVALID)
                            {
                                coherenceTable[otherCore][setIndex][searchWay] = CoherenceState::INVALID;
                            }
                            searchWay++;
                        }
                    }
                    otherCore++;
                }

                // Upgrade to modified
                invalidationCount[requestorCore]++;
                busOccupied = true;
                coherenceTable[requestorCore][setIndex][targetWay] = CoherenceState::MODIFIED;
                processorCaches[requestorCore].dirtyFlags[setIndex][targetWay] = true;
                processorCaches[requestorCore].isStalled = true;
                dataTransferQueue.push_back(BusDataTransfer{targetAddr, requestorCore, false, false, true, 0});
                pendingOperations[requestorCore] = 1;
            }
        }
    }
    
    // Process data transfers
    if (!dataTransferQueue.empty())
    {
        BusDataTransfer &currentTransfer = dataTransferQueue.front();
        
        if (currentTransfer.pendingCycles == 0)
        {
            totalBusTraffic += processorCaches[currentTransfer.destinationCore].bytesPerBlock;
            
            int destCore = currentTransfer.destinationCore;
            int transferAddr = currentTransfer.targetAddress;
            bool isWriteOp = currentTransfer.isWriteOp;
            bool isWritebackOp = currentTransfer.isWritebackOp;
            bool isInvOp = currentTransfer.isInvalidation;
            trafficBytes[destCore] += processorCaches[destCore].bytesPerBlock;
            bool evictTriggeredWb = false;
            
            if (!isWritebackOp)
            {
                int setIdx = (transferAddr >> numBlockBits) & ((1 << numSetBits) - 1);
                int tagVal = transferAddr >> (numSetBits + numBlockBits);
                
                if (isWriteOp)
                {
                    int allocatedWay = processWriteMiss(destCore, setIdx, tagVal, evictTriggeredWb);
                    coherenceTable[destCore][setIdx][allocatedWay] = CoherenceState::MODIFIED;
                }
                else if (!isInvOp)
                {
                    int allocatedWay = processReadMiss(destCore, setIdx, tagVal, evictTriggeredWb);
                    bool othersHaveData = false;
                    
                    int checkCore = 0;
                    while (checkCore < 4)
                    {
                        if (checkCore != destCore)
                        {
                            int checkWay = 0;
                            while (checkWay < associativity)
                            {
                                if (processorCaches[checkCore].tagArray[setIdx][checkWay] == tagVal && 
                                    coherenceTable[checkCore][setIdx][checkWay] != CoherenceState::INVALID)
                                {
                                    othersHaveData = true;
                                    break;
                                }
                                checkWay++;
                            }
                        }
                        if (othersHaveData)
                            break;
                        checkCore++;
                    }
                    
                    if (othersHaveData)
                    {
                        coherenceTable[destCore][setIdx][allocatedWay] = CoherenceState::SHARED;
                    }
                    else
                    {
                        coherenceTable[destCore][setIdx][allocatedWay] = CoherenceState::EXCLUSIVE;
                    }
                }
                
                processorCaches[destCore].isStalled = false;
                pendingOperations[destCore] = -1;
                
                if (evictTriggeredWb)
                {
                    processorCaches[destCore].isStalled = true;
                    pendingOperations[destCore] = 1;
                }
            }
            else
            {
                writebackCount[destCore]++;
                processorCaches[destCore].isStalled = false;
                pendingOperations[destCore] = -1;
            }

            dataTransferQueue.erase(dataTransferQueue.begin());
            if (dataTransferQueue.empty())
            {
                busOccupied = false;
            }
        }
        else
        {
            currentTransfer.pendingCycles--;
        }
    }
}
