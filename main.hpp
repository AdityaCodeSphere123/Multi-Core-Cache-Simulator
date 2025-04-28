#ifndef MAIN_HPP
#define MAIN_HPP

#include <vector>
#include <utility>

using namespace std;

// Configuration parameters for cache simulation
extern int numSetBits;      // Number of set index bits: total sets = 2^numSetBits
extern int numBlockBits;    // Number of block offset bits: block size = 2^numBlockBits bytes
extern int associativity;   // Number of lines per set (E-way associativity)

// Memory trace inputs for quad-core processor
extern vector<pair<char, const char *>> processorTrace0;
extern vector<pair<char, const char *>> processorTrace1;
extern vector<pair<char, const char *>> processorTrace2;
extern vector<pair<char, const char *>> processorTrace3;

// Cache structure for each processor core
struct CacheUnit
{
    int totalSets;          // Number of sets = 2^numSetBits
    int bytesPerBlock;      // Block size = 2^numBlockBits bytes
    bool isStalled;
    vector<vector<unsigned int>> tagArray;      // Tag storage [set][line]
    vector<vector<bool>> validBits;             // Valid bits [set][line]
    vector<vector<int>> lruOrder;               // LRU ordering [set] holds line indices
    vector<vector<bool>> dirtyFlags;            // Dirty bits [set][line]

    // Initialize cache based on global parameters
    void initialize()
    {
        totalSets = 1 << numSetBits;
        bytesPerBlock = 1 << numBlockBits;

        // Allocate and initialize cache structures
        tagArray.assign(totalSets, vector<unsigned int>(associativity, 0));
        validBits.assign(totalSets, vector<bool>(associativity, false));
        dirtyFlags.assign(totalSets, vector<bool>(associativity, false));

        // Initialize LRU ordering for each set
        lruOrder.clear();
        lruOrder.resize(totalSets);
        int setIdx = 0;
        while (setIdx < totalSets)
        {
            lruOrder[setIdx].clear();
            int lineIdx = 0;
            while (lineIdx < associativity)
            {
                lruOrder[setIdx].push_back(lineIdx);
                lineIdx++;
            }
            setIdx++;
        }
    }
};

extern CacheUnit processorCaches[4];

enum class CoherenceState
{
    MODIFIED,
    EXCLUSIVE,
    SHARED,
    INVALID
};
extern vector<vector<CoherenceState>> coherenceTable[4];

extern vector<int> executedInstructions;
extern vector<int> totalCycles;

extern vector<int> readCount;
extern vector<int> writeCount;
extern vector<int> missCount;
extern vector<int> evictionCount;
extern vector<int> writebackCount;
extern vector<int> invalidationCount;
extern vector<long long> trafficBytes;
extern vector<int> stalledCycles;
extern int busTransactionCount;
extern long long totalBusTraffic;

extern vector<bool> processorRunning;
#endif // MAIN_HPP
