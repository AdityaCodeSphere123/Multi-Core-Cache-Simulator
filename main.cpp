#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include "main.hpp"
#include "bus.hpp"
#include "cache.hpp"

using namespace std;

// Global configuration parameters
int numSetBits = 2;
int numBlockBits = 4;
int associativity = 2;

// Bus queues and data structures
vector<BusTransaction> pendingRequests;
set<int> activeWriteSet;
vector<BusDataTransfer> dataTransferQueue;
vector<int> totalCycles;
vector<int> executedInstructions;
CacheUnit processorCaches[4];
vector<vector<CoherenceState>> coherenceTable[4];

// Memory traces for each processor
vector<pair<char, const char *>> processorTrace0;
vector<pair<char, const char *>> processorTrace1;
vector<pair<char, const char *>> processorTrace2;
vector<pair<char, const char *>> processorTrace3;

// Statistics counters
vector<int> readCount(4, 0);
vector<int> writeCount(4, 0);
vector<int> missCount(4, 0);
vector<int> evictionCount(4, 0);
vector<int> writebackCount(4, 0);
vector<int> invalidationCount(4, 0);
vector<long long> trafficBytes(4, 0);
vector<int> stalledCycles(4, 0);
int busTransactionCount = 0;
long long totalBusTraffic = 0;

vector<bool> processorRunning(4, true);

// Load trace files for all four processors
bool loadProcessorTraces(const string &appPrefix)
{
    vector<vector<pair<char, const char *>> *> traceArrays = {
        &processorTrace0, &processorTrace1, &processorTrace2, &processorTrace3
    };

    int procIdx = 0;
    while (procIdx < 4)
    {
        // Build filename: app1_proc0.trace, app1_proc1.trace, etc.
        string traceFilename = appPrefix + "_proc" + to_string(procIdx) + ".trace";
        ifstream inputFile(traceFilename);

        if (!inputFile.is_open())
        {
            cerr << "Error: Could not open trace file " << traceFilename << endl;
            return false;
        }

        traceArrays[procIdx]->clear();

        string currentLine;
        while (getline(inputFile, currentLine))
        {
            // Skip empty lines and comments
            if (currentLine.empty() || currentLine[0] == '#')
            {
                continue;
            }

            // Parse format: "R 0x817b08" or "W 0x817b08"
            istringstream lineParser(currentLine);
            char operation;
            string hexAddress;

            if (lineParser >> operation >> hexAddress)
            {
                if (operation == 'R' || operation == 'W')
                {
                    // Create persistent copy of address string
                    char *addressBuffer = new char[hexAddress.length() + 1];
                    strcpy(addressBuffer, hexAddress.c_str());
                    traceArrays[procIdx]->push_back({operation, addressBuffer});
                }
            }
        }

        inputFile.close();
        procIdx++;
    }

    return true;
}

void runMulticoreSimulation()
{
    // Track current position in each processor's trace
    vector<size_t> tracePosition(4, 0);

    // Array of trace references
    const vector<vector<pair<char, const char *>>> allTraces = {
        processorTrace0, processorTrace1, processorTrace2, processorTrace3
    };

    // Simulation control
    bool simulationActive = true;
    int currentCycle = 0;
    int peakCycles = 0;

    while (simulationActive)
    {
        // Process each processor in round-robin order
        int procId = 0;
        while (procId < 4)
        {
            // Skip completed processors
            if (!processorRunning[procId])
            {
                procId++;
                continue;
            }

            // Check if processor has remaining instructions
            if (tracePosition[procId] < allTraces[procId].size())
            {
                pair<char, const char *> currentOp = allTraces[procId][tracePosition[procId]];
                executeMemoryOperation(currentOp, procId);
            }
            else
            {
                processorRunning[procId] = false;
            }
            procId++;
        }

        processBusTransactions();

        // Advance trace position for non-stalled processors
        int updateIdx = 0;
        while (updateIdx < 4)
        {
            if (!processorCaches[updateIdx].isStalled && processorRunning[updateIdx])
            {
                tracePosition[updateIdx]++;
                executedInstructions[updateIdx]++;
                if (tracePosition[updateIdx] == allTraces[updateIdx].size())
                {
                    processorRunning[updateIdx] = false;
                }
            }
            updateIdx++;
        }

        // Check if simulation should continue
        simulationActive = false;
        int checkIdx = 0;
        while (checkIdx < 4)
        {
            if (processorRunning[checkIdx] || processorCaches[checkIdx].isStalled || !dataTransferQueue.empty())
            {
                simulationActive = true;
                break;
            }
            checkIdx++;
        }

        currentCycle++;
        peakCycles = max(peakCycles, currentCycle);
    }

    // Count reads and writes from traces
    int countIdx = 0;
    while (countIdx < 4)
    {
        size_t opIdx = 0;
        while (opIdx < allTraces[countIdx].size())
        {
            if (allTraces[countIdx][opIdx].first == 'R')
            {
                readCount[countIdx]++;
            }
            else if (allTraces[countIdx][opIdx].first == 'W')
            {
                writeCount[countIdx]++;
            }
            opIdx++;
        }
        countIdx++;
    }

    // Calculate totals
    int blockBytes = 1 << numBlockBits;
    int setCount = 1 << numSetBits;
    double cacheSizeKB = (setCount * associativity * blockBytes) / 1024.0;
    
    int totalInstructions = 0;
    int totalReads = 0;
    int totalWrites = 0;
    int totalMisses = 0;
    int totalEvictions = 0;
    int totalWritebacks = 0;
    int totalInvalidations = 0;
    long long totalDataTraffic = 0;
    
    int calcIdx = 0;
    while (calcIdx < 4)
    {
        totalInstructions += executedInstructions[calcIdx];
        totalReads += readCount[calcIdx];
        totalWrites += writeCount[calcIdx];
        totalMisses += missCount[calcIdx];
        totalEvictions += evictionCount[calcIdx];
        totalWritebacks += writebackCount[calcIdx];
        totalInvalidations += invalidationCount[calcIdx];
        totalDataTraffic += trafficBytes[calcIdx];
        calcIdx++;
    }

    // Print simulation results
    cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    cout << "║           MULTICORE CACHE SIMULATOR - SIMULATION REPORT          ║\n";
    cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    cout << "┌──────────────────────────────────────────────────────────────────┐\n";
    cout << "│                     SIMULATION PARAMETERS                        │\n";
    cout << "├──────────────────────────────────────────────────────────────────┤\n";
    cout << "│  Set Index Bits (s):        " << setw(8) << numSetBits << "                            │\n";
    cout << "│  Associativity (E):         " << setw(8) << associativity << "                            │\n";
    cout << "│  Block Bits (b):            " << setw(8) << numBlockBits << "                            │\n";
    cout << "│  Block Size:                " << setw(5) << blockBytes << " bytes                        │\n";
    cout << "│  Number of Sets:            " << setw(8) << setCount << "                            │\n";
    cout << fixed << setprecision(2);
    cout << "│  Cache Size (per core):     " << setw(5) << cacheSizeKB << " KB                          │\n";
    cout << "│  Total Cache Size:          " << setw(5) << cacheSizeKB * 4 << " KB                          │\n";
    cout << "├──────────────────────────────────────────────────────────────────┤\n";
    cout << "│  Coherence Protocol:        MESI (Illinois)                      │\n";
    cout << "│  Write Policy:              Write-back, Write-allocate           │\n";
    cout << "│  Replacement Policy:        LRU (Least Recently Used)            │\n";
    cout << "│  Bus Architecture:          Central Snooping Bus                 │\n";
    cout << "│  Number of Cores:           4                                    │\n";
    cout << "└──────────────────────────────────────────────────────────────────┘\n\n";

    cout << "┌──────────────────────────────────────────────────────────────────┐\n";
    cout << "│                     PER-CORE STATISTICS                          │\n";
    cout << "└──────────────────────────────────────────────────────────────────┘\n\n";

    int statIdx = 0;
    while (statIdx < 4)
    {
        double missPercent = (readCount[statIdx] + writeCount[statIdx] > 0) 
            ? (missCount[statIdx] * 100.0) / (readCount[statIdx] + writeCount[statIdx]) : 0.0;
        double hitPercent = 100.0 - missPercent;
        double readPercent = (readCount[statIdx] + writeCount[statIdx] > 0)
            ? (readCount[statIdx] * 100.0) / (readCount[statIdx] + writeCount[statIdx]) : 0.0;
        double writePercent = 100.0 - readPercent;
        int cacheHits = readCount[statIdx] + writeCount[statIdx] - missCount[statIdx];
        double ipc = (totalCycles[statIdx] + executedInstructions[statIdx] > 0)
            ? (double)executedInstructions[statIdx] / (totalCycles[statIdx] + executedInstructions[statIdx]) : 0.0;

        cout << "┌─────────────────────── CORE " << statIdx << " ───────────────────────────────────┐\n";
        cout << "│  Memory Access Summary:                                          │\n";
        cout << "│    Total Instructions:      " << setw(12) << executedInstructions[statIdx] << "                      │\n";
        cout << "│    Total Reads:             " << setw(12) << readCount[statIdx] << " (" << setw(5) << fixed << setprecision(2) << readPercent << "%)               │\n";
        cout << "│    Total Writes:            " << setw(12) << writeCount[statIdx] << " (" << setw(5) << writePercent << "%)               │\n";
        cout << "│                                                                  │\n";
        cout << "│  Cache Performance:                                              │\n";
        cout << "│    Cache Hits:              " << setw(12) << cacheHits << "                      │\n";
        cout << "│    Cache Misses:            " << setw(12) << missCount[statIdx] << "                      │\n";
        cout << fixed << setprecision(5);
        cout << "│    Hit Rate:                " << setw(11) << hitPercent << "%                      │\n";
        cout << "│    Miss Rate:               " << setw(11) << missPercent << "%                      │\n";
        cout << "│                                                                  │\n";
        cout << "│  Cache Events:                                                   │\n";
        cout << "│    Evictions:               " << setw(12) << evictionCount[statIdx] << "                      │\n";
        cout << "│    Writebacks:              " << setw(12) << writebackCount[statIdx] << "                      │\n";
        cout << "│    Bus Invalidations:       " << setw(12) << invalidationCount[statIdx] << "                      │\n";
        cout << "│                                                                  │\n";
        cout << "│  Timing & Traffic:                                               │\n";
        cout << "│    Execution Cycles:        " << setw(12) << totalCycles[statIdx] + executedInstructions[statIdx] << "                      │\n";
        cout << "│    Idle/Stall Cycles:       " << setw(12) << stalledCycles[statIdx] << "                      │\n";
        cout << fixed << setprecision(4);
        cout << "│    IPC (approx):            " << setw(12) << ipc << "                      │\n";
        cout << "│    Data Traffic:            " << setw(9) << trafficBytes[statIdx] << " bytes                 │\n";
        cout << "└──────────────────────────────────────────────────────────────────┘\n\n";
        statIdx++;
    }

    double overallMissRate = (totalReads + totalWrites > 0) 
        ? (totalMisses * 100.0) / (totalReads + totalWrites) : 0.0;
    double overallHitRate = 100.0 - overallMissRate;

    cout << "┌──────────────────────────────────────────────────────────────────┐\n";
    cout << "│                     AGGREGATE STATISTICS                         │\n";
    cout << "├──────────────────────────────────────────────────────────────────┤\n";
    cout << "│  Total Instructions (all cores):    " << setw(14) << totalInstructions << "            │\n";
    cout << "│  Total Memory Accesses:             " << setw(14) << totalReads + totalWrites << "            │\n";
    cout << "│  Total Reads:                       " << setw(14) << totalReads << "            │\n";
    cout << "│  Total Writes:                      " << setw(14) << totalWrites << "            │\n";
    cout << "│  Total Cache Hits:                  " << setw(14) << (totalReads + totalWrites - totalMisses) << "            │\n";
    cout << "│  Total Cache Misses:                " << setw(14) << totalMisses << "            │\n";
    cout << fixed << setprecision(5);
    cout << "│  Overall Hit Rate:                  " << setw(13) << overallHitRate << "%            │\n";
    cout << "│  Overall Miss Rate:                 " << setw(13) << overallMissRate << "%            │\n";
    cout << "│  Total Evictions:                   " << setw(14) << totalEvictions << "            │\n";
    cout << "│  Total Writebacks:                  " << setw(14) << totalWritebacks << "            │\n";
    cout << "│  Total Invalidations:               " << setw(14) << totalInvalidations << "            │\n";
    cout << "└──────────────────────────────────────────────────────────────────┘\n\n";

    cout << "┌──────────────────────────────────────────────────────────────────┐\n";
    cout << "│                     BUS & COHERENCE SUMMARY                      │\n";
    cout << "├──────────────────────────────────────────────────────────────────┤\n";
    cout << "│  Total Bus Transactions:            " << setw(14) << busTransactionCount << "            │\n";
    cout << "│  Total Bus Traffic:                 " << setw(11) << totalBusTraffic << " bytes         │\n";
    cout << "│  Total Core Data Traffic:           " << setw(11) << totalDataTraffic << " bytes         │\n";
    double avgBusTransPerInstr = (totalInstructions > 0) 
        ? (double)busTransactionCount / totalInstructions : 0.0;
    cout << fixed << setprecision(6);
    cout << "│  Bus Transactions per Instruction:  " << setw(14) << avgBusTransPerInstr << "            │\n";
    cout << "└──────────────────────────────────────────────────────────────────┘\n\n";

    cout << "┌──────────────────────────────────────────────────────────────────┐\n";
    cout << "│                     TIMING SUMMARY                               │\n";
    cout << "├──────────────────────────────────────────────────────────────────┤\n";
    cout << "│  Total Simulation Cycles:           " << setw(14) << currentCycle - 1 << "            │\n";
    cout << "│  Maximum Execution Time:            " << setw(14) << peakCycles << "            │\n";
    cout << "└──────────────────────────────────────────────────────────────────┘\n";
}

void displayUsageHelp(const char *programName)
{
    cout << "Usage: " << programName << " -t <tracefile> -s <s> -E <E> -b <b> [-o <outfilename>] [-h]\n"
         << "\nOptions:\n"
         << "  -t <tracefile>  Name of the parallel application (e.g. app1) whose 4 traces are\n"
         << "                  to be used in simulation.\n"
         << "  -s <s>          Number of set index bits (number of sets in the cache = S = 2^s).\n"
         << "  -E <E>          Associativity (number of cache lines per set).\n"
         << "  -b <b>          Number of block bits (block size = B = 2^b).\n"
         << "  -o <outfilename>Log output in file for plotting etc.\n"
         << "  -h              Print this help message.\n";
}

int main(int argc, char *argv[])
{
    string applicationPrefix;
    string outputFilename;

    // Parse command line arguments
    int argIdx = 1;
    while (argIdx < argc)
    {
        if (strcmp(argv[argIdx], "-h") == 0)
        {
            displayUsageHelp(argv[0]);
            return 0;
        }
        else if (strcmp(argv[argIdx], "-t") == 0)
        {
            if (argIdx + 1 < argc)
            {
                applicationPrefix = argv[++argIdx];
            }
            else
            {
                cerr << "Error: Missing argument for -t option.\n";
                return 1;
            }
        }
        else if (strcmp(argv[argIdx], "-s") == 0)
        {
            if (argIdx + 1 < argc)
            {
                numSetBits = atoi(argv[++argIdx]);
            }
            else
            {
                cerr << "Error: Missing argument for -s option.\n";
                return 1;
            }
        }
        else if (strcmp(argv[argIdx], "-E") == 0)
        {
            if (argIdx + 1 < argc)
            {
                associativity = atoi(argv[++argIdx]);
            }
            else
            {
                cerr << "Error: Missing argument for -E option.\n";
                return 1;
            }
        }
        else if (strcmp(argv[argIdx], "-b") == 0)
        {
            if (argIdx + 1 < argc)
            {
                numBlockBits = atoi(argv[++argIdx]);
            }
            else
            {
                cerr << "Error: Missing argument for -b option.\n";
                return 1;
            }
        }
        else if (strcmp(argv[argIdx], "-o") == 0)
        {
            if (argIdx + 1 < argc)
            {
                outputFilename = argv[++argIdx];
            }
            else
            {
                cerr << "Error: Missing argument for -o option.\n";
                return 1;
            }
            cout << "Output file name: " << argv[argIdx] << endl;
        }
        else
        {
            cerr << "Error: Unknown option " << argv[argIdx] << ".\n";
            return 1;
        }
        argIdx++;
    }

    // Validate required arguments
    if (applicationPrefix.empty())
    {
        cerr << "Error: Trace file prefix (-t) is required.\n";
        displayUsageHelp(argv[0]);
        return 1;
    }

    // Load trace files
    if (!loadProcessorTraces(applicationPrefix))
    {
        cerr << "Error loading trace files. Exiting.\n";
        return 1;
    }

    // Initialize caches and coherence state
    int initIdx = 0;
    while (initIdx < 4)
    {
        processorCaches[initIdx].initialize();
        coherenceTable[initIdx].assign(1 << numSetBits, vector<CoherenceState>(associativity, CoherenceState::INVALID));
        initIdx++;
    }

    // Initialize counters
    executedInstructions.assign(4, 0);
    totalCycles.assign(4, 0);

    // Handle output redirection
    ofstream outputFile;
    if (!outputFilename.empty())
    {
        outputFile.open(outputFilename);
        if (!outputFile.is_open())
        {
            cerr << "Error: Could not open output file " << outputFilename << endl;
            return 1;
        }
        streambuf *originalBuffer = cout.rdbuf();
        cout.rdbuf(outputFile.rdbuf());

        runMulticoreSimulation();

        cout.rdbuf(originalBuffer);
        outputFile.close();
    }
    else
    {
        runMulticoreSimulation();
    }

    // Cleanup allocated memory
    auto cleanupIt = processorTrace0.begin();
    while (cleanupIt != processorTrace0.end())
    {
        delete[] cleanupIt->second;
        cleanupIt++;
    }
    cleanupIt = processorTrace1.begin();
    while (cleanupIt != processorTrace1.end())
    {
        delete[] cleanupIt->second;
        cleanupIt++;
    }
    cleanupIt = processorTrace2.begin();
    while (cleanupIt != processorTrace2.end())
    {
        delete[] cleanupIt->second;
        cleanupIt++;
    }
    cleanupIt = processorTrace3.begin();
    while (cleanupIt != processorTrace3.end())
    {
        delete[] cleanupIt->second;
        cleanupIt++;
    }

    return 0;
}
