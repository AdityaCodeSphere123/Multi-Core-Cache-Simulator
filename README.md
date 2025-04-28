# Multicore Cache Simulator with MESI Protocol

**Author:** Aditya Anand  
**Course:** Computer Architecture  
**Semester:** 2024-25  
**Date:** April 2025

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [System Architecture](#2-system-architecture)
3. [Implementation Details](#3-implementation-details)
4. [MESI Cache Coherence Protocol](#4-mesi-cache-coherence-protocol)
5. [Data Structures](#5-data-structures)
6. [Algorithm and Simulation Flow](#6-algorithm-and-simulation-flow)
7. [Building and Usage](#7-building-and-usage)
8. [Experimental Results](#8-experimental-results)
9. [Analysis and Discussion](#9-analysis-and-discussion)
10. [Conclusion](#10-conclusion)
11. [References](#11-references)

---

## 1. Introduction

### 1.1 Problem Statement

Modern processors employ multiple cores to achieve higher performance through parallelism. Each core typically has its own private cache to reduce memory access latency. However, when multiple caches hold copies of the same memory location, maintaining consistency becomes critical. This is known as the **cache coherence problem**.

### 1.2 Objectives

This project implements a multicore cache simulator with the following goals:

- Simulate a quad-core processor with private L1 caches
- Implement the MESI cache coherence protocol
- Analyze cache performance under various configurations
- Study the impact of cache parameters on system behavior

### 1.3 Scope

The simulator supports:
- Configurable cache size, associativity, and block size
- Memory trace-driven simulation for four processors
- Detailed performance statistics collection
- Analysis of cache hits, misses, evictions, and coherence traffic

---

## 2. System Architecture

### 2.1 Hardware Model

The simulator models a symmetric multiprocessor (SMP) system with the following components:

```
┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐
│   Core 0    │  │   Core 1    │  │   Core 2    │  │   Core 3    │
│  ┌───────┐  │  │  ┌───────┐  │  │  ┌───────┐  │  │  ┌───────┐  │
│  │L1 Cache│  │  │  │L1 Cache│  │  │  │L1 Cache│  │  │  │L1 Cache│  │
│  └───┬───┘  │  │  └───┬───┘  │  │  └───┬───┘  │  │  └───┬───┘  │
└──────┼──────┘  └──────┼──────┘  └──────┼──────┘  └──────┼──────┘
       │                │                │                │
       └────────────────┴────────────────┴────────────────┘
                                │
                    ┌───────────┴───────────┐
                    │   Snooping Bus        │
                    └───────────┬───────────┘
                                │
                    ┌───────────┴───────────┐
                    │    Main Memory        │
                    └───────────────────────┘
```

### 2.2 System Specifications

| Component | Specification |
|-----------|---------------|
| Number of Cores | 4 |
| Cache Type | Private L1 (per core) |
| Cache Organization | Set-associative |
| Coherence Protocol | MESI (Illinois Protocol) |
| Write Policy | Write-back, Write-allocate |
| Replacement Policy | LRU (Least Recently Used) |
| Bus Architecture | Central snooping bus |
| Address Size | 32 bits |

### 2.3 Configurable Parameters

| Parameter | Symbol | Description | Formula |
|-----------|--------|-------------|---------|
| Set Index Bits | s | Number of bits for set indexing | Sets = 2^s |
| Block Bits | b | Number of bits for block offset | Block Size = 2^b bytes |
| Associativity | E | Number of cache lines per set | Ways = E |

**Cache Size Calculation:**
```
Cache Size = (Number of Sets) × (Associativity) × (Block Size)
           = 2^s × E × 2^b bytes
```

---

## 3. Implementation Details

### 3.1 File Organization

| File | Purpose |
|------|---------|
| `main.cpp` | Main simulation driver, argument parsing, I/O handling |
| `main.hpp` | Global data structures, cache structure definition |
| `cache.cpp` | Cache operations: hit/miss detection, LRU management |
| `cache.hpp` | Cache function prototypes |
| `bus.cpp` | Bus transaction handling, MESI state transitions |
| `bus.hpp` | Bus-related structures and enumerations |
| `makefile` | Build configuration |
| `plot_graphs.py` | Visualization scripts for results |

### 3.2 Memory Trace Format

The simulator reads memory traces in the following format:
```
<operation> <hex_address>
```

Where:
- `operation`: `R` (read) or `W` (write)
- `hex_address`: 32-bit hexadecimal memory address (e.g., `0x817b08`)

**Example trace file (`app1_proc0.trace`):**
```
R 0x817b08
W 0x817b10
R 0x817b18
R 0x817b20
W 0x817b08
```

### 3.3 Address Decomposition

A 32-bit address is decomposed as follows:

```
┌─────────────────────┬──────────────────┬─────────────────┐
│       Tag           │   Set Index      │  Block Offset   │
│   (32 - s - b bits) │    (s bits)      │    (b bits)     │
└─────────────────────┴──────────────────┴─────────────────┘
```

**Extraction formulas:**
```cpp
int setIndex = (address >> numBlockBits) & ((1 << numSetBits) - 1);
int tag = address >> (numSetBits + numBlockBits);
```

---

## 4. MESI Cache Coherence Protocol

### 4.1 State Definitions

The MESI protocol defines four states for each cache line:

| State | Name | Description | Properties |
|-------|------|-------------|------------|
| **M** | Modified | Exclusively owned, dirty | Only copy, modified from memory |
| **E** | Exclusive | Exclusively owned, clean | Only copy, matches memory |
| **S** | Shared | Shared with other caches | Read-only, may exist in others |
| **I** | Invalid | Not valid | Cache line is empty/stale |

### 4.2 State Transition Diagram

```
                    ┌──────────────────────────────────────┐
                    │                                      │
                    ▼                                      │
              ┌─────────┐                                  │
     ┌───────►│ INVALID │◄────────────────────┐           │
     │        └────┬────┘                     │           │
     │             │                          │           │
     │    PrRd     │    PrWr                  │           │
     │   (BusRd)   │   (BusRdX)               │           │
     │             │                          │           │
     │             ▼                          │           │
     │   ┌─────────────────────┐              │           │
     │   │                     │              │           │
     │   ▼                     ▼              │           │
┌────┴───────┐           ┌───────────┐        │           │
│  EXCLUSIVE │           │ MODIFIED  │        │           │
│    (E)     │           │    (M)    │────────┤           │
└─────┬──────┘           └─────┬─────┘        │           │
      │                        │         BusRdX│           │
      │ PrWr                   │         (Flush)           │
      │ (silent)               │              │           │
      │                        │              │           │
      └────────►┌──────────────┘              │           │
                │                             │           │
           BusRd│                             │           │
          (Share)                             │           │
                │                             │           │
                ▼                             │           │
          ┌───────────┐                       │           │
          │  SHARED   │───────────────────────┘           │
          │    (S)    │                                   │
          └─────┬─────┘                                   │
                │                                         │
                │ BusRdX / BusUpgr                        │
                │ (Invalidate)                            │
                └─────────────────────────────────────────┘
```

### 4.3 Bus Transactions

| Transaction | Trigger | Purpose | Action by Others |
|-------------|---------|---------|------------------|
| **BusRd** | Read miss | Request data for reading | Share if have copy |
| **BusRdX** | Write miss | Request exclusive copy | Invalidate copies |
| **BusUpgr** | Write hit on Shared | Upgrade to Modified | Invalidate copies |

### 4.4 Processor Actions and State Transitions

#### On Processor Read (PrRd):

| Current State | Action | New State |
|---------------|--------|-----------|
| Modified | Read hit | Modified |
| Exclusive | Read hit | Exclusive |
| Shared | Read hit | Shared |
| Invalid | Issue BusRd | Exclusive (if no sharers) or Shared |

#### On Processor Write (PrWr):

| Current State | Action | New State |
|---------------|--------|-----------|
| Modified | Write hit | Modified |
| Exclusive | Write hit (silent) | Modified |
| Shared | Issue BusUpgr, invalidate others | Modified |
| Invalid | Issue BusRdX, invalidate others | Modified |

### 4.5 Snooping Actions

When a cache observes a bus transaction:

| Bus Transaction | Current State | Snoop Action | New State |
|-----------------|---------------|--------------|-----------|
| BusRd | Modified | Supply data, writeback | Shared |
| BusRd | Exclusive | Supply data | Shared |
| BusRd | Shared | No action | Shared |
| BusRdX | Modified | Supply data, writeback | Invalid |
| BusRdX | Exclusive | No action | Invalid |
| BusRdX | Shared | No action | Invalid |
| BusUpgr | Shared | Invalidate | Invalid |

---

## 5. Data Structures

### 5.1 Cache Unit Structure

```cpp
struct CacheUnit {
    int totalSets;                              // Number of sets = 2^s
    int bytesPerBlock;                          // Block size = 2^b bytes
    bool isStalled;                             // Processor stall flag
    vector<vector<unsigned int>> tagArray;      // Tag storage [set][way]
    vector<vector<bool>> validBits;             // Valid bits [set][way]
    vector<vector<int>> lruOrder;               // LRU tracking [set]
    vector<vector<bool>> dirtyFlags;            // Modified bits [set][way]
    
    void initialize();                          // Initialize cache structures
};
```

### 5.2 Coherence State Enumeration

```cpp
enum class CoherenceState {
    MODIFIED,   // M - Exclusive, dirty
    EXCLUSIVE,  // E - Exclusive, clean
    SHARED,     // S - Shared with others
    INVALID     // I - Not valid
};
```

### 5.3 Bus Transaction Structures

```cpp
enum class BusRequestType {
    READ_SHARED,        // BusRd - Read request
    READ_EXCLUSIVE,     // BusRdX - Read for exclusive/write
    UPGRADE_REQUEST     // BusUpgr - Upgrade Shared to Modified
};

struct BusTransaction {
    int requestorId;            // Requesting processor ID
    int memoryAddress;          // Target memory address
    BusRequestType reqType;     // Type of bus request
};

struct BusDataTransfer {
    int targetAddress;          // Memory address
    int destinationCore;        // Receiving processor
    bool isWriteOp;             // Write operation flag
    bool isWritebackOp;         // Writeback flag
    bool isInvalidation;        // Invalidation signal
    int pendingCycles;          // Remaining transfer cycles
};
```

### 5.4 Global Statistics

```cpp
vector<int> readCount(4, 0);           // Reads per core
vector<int> writeCount(4, 0);          // Writes per core
vector<int> missCount(4, 0);           // Cache misses per core
vector<int> evictionCount(4, 0);       // Evictions per core
vector<int> writebackCount(4, 0);      // Writebacks per core
vector<int> invalidationCount(4, 0);   // Invalidations per core
vector<long long> trafficBytes(4, 0);  // Data traffic per core
vector<int> stalledCycles(4, 0);       // Stall cycles per core
int busTransactionCount = 0;           // Total bus transactions
long long totalBusTraffic = 0;         // Total bus traffic bytes
```

---

## 6. Algorithm and Simulation Flow

### 6.1 Main Simulation Loop

```
1. Load trace files for all 4 processors
2. Initialize cache structures and coherence states
3. While (any processor active OR bus busy):
   a. For each processor (round-robin):
      - If not stalled and has instructions:
        * Execute next memory operation
        * Handle hit/miss accordingly
   b. Process bus transactions
   c. Advance non-stalled processors
4. Collect and report statistics
```

### 6.2 Cache Access Algorithm

```
executeMemoryOperation(operation, address, processorId):
    
    setIndex = extractSetIndex(address)
    tag = extractTag(address)
    
    IF operation == READ:
        Search for tag match in set
        IF hit:
            Update LRU ordering
            Return (no stall)
        ELSE:
            Issue BusRd request
            Stall processor
            
    ELSE IF operation == WRITE:
        Search for tag match in set
        IF hit:
            IF state == MODIFIED or EXCLUSIVE:
                Update LRU, mark dirty
                Transition to MODIFIED (if E)
            ELSE IF state == SHARED:
                Issue BusUpgr request
        ELSE:
            Issue BusRdX request
            Stall processor
```

### 6.3 Miss Handling Algorithm

```
processReadMiss(processorId, setIndex, tag):
    
    // Find target way
    IF invalid line exists:
        targetWay = first invalid line
    ELSE:
        targetWay = LRU line
        IF dirty:
            Schedule writeback
        Increment eviction count
    
    // Update cache
    tags[setIndex][targetWay] = tag
    Update LRU ordering
    Return targetWay
```

### 6.4 Bus Transaction Processing

```
processBusTransactions():
    
    FOR each pending request:
        IF bus busy:
            Continue
            
        SWITCH request.type:
            CASE BusRd:
                Check other caches for data
                IF found in M state:
                    Downgrade to S, writeback
                ELSE IF found in E state:
                    Downgrade to S
                Schedule data transfer
                
            CASE BusRdX:
                Invalidate copies in other caches
                IF found in M state:
                    Writeback first
                Schedule data transfer
                
            CASE BusUpgr:
                Invalidate copies in other caches
                Upgrade requestor to M state
    
    Process data transfer queue
    Update stall status
```

---

## 7. Building and Usage

### 7.1 Prerequisites

- C++ compiler with C++11 support (g++ recommended)
- Make build system
- Python 3 with matplotlib (for visualization)

### 7.2 Quick Start

```bash
# Step 1: Unzip the trace files
unzip traces.zip

# Step 2: Build the simulator
make

# Step 3: Run the simulation
./L1simulate -t ./traces/app1 -s 6 -E 4 -b 5
```

### 7.3 Compilation

```bash
# Build the simulator
make

# Clean build artifacts
make clean
```

### 7.4 Command Line Interface

```bash
./L1simulate -t <trace_prefix> -s <s> -E <E> -b <b> [-o <output>] [-h]
```

| Option | Required | Description |
|--------|----------|-------------|
| `-t <prefix>` | Yes | Trace file prefix (loads `<prefix>_proc[0-3].trace`) |
| `-s <bits>` | Yes | Number of set index bits |
| `-E <ways>` | Yes | Associativity (ways per set) |
| `-b <bits>` | Yes | Number of block offset bits |
| `-o <file>` | No | Output file for results |
| `-h` | No | Display help message |

### 7.5 Example Usage

```bash
# Run with app1 traces (after unzipping)
./L1simulate -t ./traces/app1 -s 6 -E 4 -b 5

# Run with app2 traces
./L1simulate -t ./traces/app2 -s 6 -E 4 -b 5

# Save results to file
./L1simulate -t ./traces/app1 -s 6 -E 4 -b 5 -o results.txt

# Different cache configurations
./L1simulate -t ./traces/app1 -s 4 -E 2 -b 4    # 512B cache, 2-way, 16B blocks
./L1simulate -t ./traces/app1 -s 8 -E 8 -b 6    # 128KB cache, 8-way, 64B blocks
```

### 7.6 Trace File Requirements

Trace files must be named: `<prefix>_proc0.trace`, `<prefix>_proc1.trace`, `<prefix>_proc2.trace`, `<prefix>_proc3.trace`

Each line format: `<R|W> <hex_address>`

The provided `traces.zip` contains:
- `app1_proc[0-3].trace` - Application 1 traces for 4 cores
- `app2_proc[0-3].trace` - Application 2 traces for 4 cores

---

## 8. Experimental Results

### 8.1 Test Configuration

The following experiments analyze cache behavior under various configurations using provided trace files.

### 8.2 Effect of Set Index Bits (Cache Size)

![Set Index Bits Analysis](interesting_traces/plots/set_index_bits.png)

**Configuration:** Varying s (2-8), E=4, b=5

| Set Bits (s) | Number of Sets | Cache Size | Expected Behavior |
|--------------|----------------|------------|-------------------|
| 2 | 4 | 0.5 KB | High conflict misses |
| 4 | 16 | 2 KB | Moderate performance |
| 6 | 64 | 8 KB | Good performance |
| 8 | 256 | 32 KB | Low miss rate |

**Observation:** Increasing set index bits exponentially increases cache capacity, significantly reducing miss rates. Each additional bit doubles the number of sets, effectively doubling cache size while maintaining the same associativity.

![Set Index Bits Analysis 2](interesting_traces/plots/set_index_bits2.png)

![Set Index Constant E](interesting_traces/plots/set_index_bits_const_e_4096.png)

### 8.3 Effect of Associativity

![Associativity Analysis](interesting_traces/plots/associativity.png)

**Configuration:** s=6, Varying E (1-8), b=5

| Associativity | Type | Characteristics |
|---------------|------|-----------------|
| 1 | Direct-mapped | Simple, high conflict misses |
| 2 | 2-way set associative | Significant improvement |
| 4 | 4-way set associative | Good balance |
| 8 | 8-way set associative | Diminishing returns |

**Observation:** Higher associativity reduces conflict misses by providing more placement options for each memory block. However, benefits diminish beyond 4-way associativity while hardware complexity increases.

![Associativity Analysis 2](interesting_traces/plots/associativity2.png)

![Associativity Constant](interesting_traces/plots/associativity_const_e_4096.png)

### 8.4 Effect of Block Size

![Block Size Analysis](interesting_traces/plots/block_bits.png)

**Configuration:** s=6, E=4, Varying b (3-7)

| Block Bits (b) | Block Size | Trade-offs |
|----------------|------------|------------|
| 3 | 8 bytes | Low spatial locality exploitation |
| 4 | 16 bytes | Moderate |
| 5 | 32 bytes | Good balance |
| 6 | 64 bytes | High bandwidth usage |
| 7 | 128 bytes | Potential wastage, high miss penalty |

**Observation:** Larger blocks exploit spatial locality by fetching adjacent data that may be accessed soon. However, excessively large blocks increase miss penalty and may cause cache pollution if spatial locality is low.

![Block Size Analysis 2](interesting_traces/plots/block_bits2.png)

![Block Bits Constant](interesting_traces/plots/block_bits_const_4096.png)

### 8.5 Overall Cache Size Impact

![Cache Size Analysis](interesting_traces/plots/cache_size.png)

![Cache Size Analysis 2](interesting_traces/plots/cache_size2.png)

**Observation:** Total cache size has the most significant impact on miss rate. Doubling cache size typically yields 30-50% reduction in cache misses, though benefits plateau as working set fits entirely in cache.

---

## 9. Analysis and Discussion

### 9.1 Key Findings

#### 9.1.1 Cache Size Dominance

Cache capacity is the primary factor affecting miss rates:
- Capacity misses dominate when working set exceeds cache size
- Each doubling of cache size provides substantial improvement
- Beyond working set size, further increases provide minimal benefit

#### 9.1.2 Associativity Trade-offs

| Associativity | Pros | Cons |
|---------------|------|------|
| Low (1-2) | Simple hardware, fast access | High conflict misses |
| Medium (4) | Good balance | Moderate complexity |
| High (8+) | Low conflict misses | Complex comparators, diminishing returns |

**Recommendation:** 4-way associativity provides optimal trade-off for most workloads.

#### 9.1.3 Block Size Optimization

| Block Size | Best For | Avoid When |
|------------|----------|------------|
| Small (8-16B) | Random access patterns | Sequential access |
| Medium (32-64B) | Mixed workloads | - |
| Large (128B+) | Streaming/sequential | Random access patterns |

**Recommendation:** 32-64 byte blocks match typical memory access patterns.

### 9.2 MESI Protocol Overhead

The coherence protocol introduces additional traffic:

| Traffic Type | Cause | Impact |
|--------------|-------|--------|
| Invalidations | Write to shared data | Bandwidth consumption |
| Writebacks | Modified line eviction | Memory bandwidth |
| Data sharing | Read miss with sharer | Cache-to-cache transfer |

**Observation:** Applications with high data sharing see increased coherence traffic, but MESI minimizes unnecessary traffic through the Exclusive state.

### 9.3 Performance Metrics Summary

For a typical configuration (s=6, E=4, b=5, 8KB cache):

| Metric | Description |
|--------|-------------|
| Miss Rate | Percentage of accesses that miss |
| Eviction Rate | Cache line replacements |
| Writeback Rate | Dirty evictions requiring memory write |
| Bus Utilization | Coherence traffic overhead |

### 9.4 Design Recommendations

| Parameter | Recommended Value | Reasoning |
|-----------|-------------------|-----------|
| Cache Size | 16-64 KB/core | Balance cost and hit rate |
| Associativity | 4-8 way | Reduce conflicts without excess complexity |
| Block Size | 32-64 bytes | Match spatial locality patterns |
| Write Policy | Write-back | Reduce memory traffic |

---

## 10. Conclusion

### 10.1 Summary

This project successfully implements a multicore cache simulator with the MESI coherence protocol. Key achievements include:

1. **Accurate Simulation:** The simulator correctly models cache behavior including hits, misses, evictions, and coherence state transitions.

2. **Flexible Configuration:** Parameterized design allows exploration of various cache configurations.

3. **Comprehensive Statistics:** Detailed metrics enable thorough performance analysis.

4. **MESI Implementation:** Full implementation of the Illinois MESI protocol with all state transitions and bus transactions.

### 10.2 Insights Gained

- Cache size is the dominant factor in reducing miss rates
- Associativity beyond 4-way provides diminishing returns
- Block size should match application's spatial locality
- Coherence traffic can significantly impact multicore performance

### 10.3 Future Enhancements

Potential improvements for the simulator:

1. **Multi-level Cache:** Add L2/L3 cache simulation
2. **Alternative Protocols:** Implement MOESI, Dragon, or directory-based protocols
3. **Out-of-Order Execution:** Model processor pipelines
4. **Memory System:** Add realistic memory latency and bandwidth models
5. **Visualization:** Real-time cache state visualization

---

## 11. References

1. Patterson, D. A., & Hennessy, J. L. (2017). *Computer Organization and Design: The Hardware/Software Interface* (5th ed.). Morgan Kaufmann.

2. Handy, J. (1998). *The Cache Memory Book* (2nd ed.). Academic Press.

3. Culler, D. E., Singh, J. P., & Gupta, A. (1999). *Parallel Computer Architecture: A Hardware/Software Approach*. Morgan Kaufmann.

4. Sorin, D. J., Hill, M. D., & Wood, D. A. (2011). *A Primer on Memory Consistency and Cache Coherence*. Morgan & Claypool.

5. Papamarcos, M. S., & Patel, J. H. (1984). A low-overhead coherence solution for multiprocessors with private cache memories. *ACM SIGARCH Computer Architecture News*, 12(3), 348-354.

---


**Author:** Aditya Anand  
**Course:** COL216 - Computer Architecture  
**Institution:** Indian Institute of Technology Delhi  
**Date:** December 2024

