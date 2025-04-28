#ifndef BUS_HPP
#define BUS_HPP

#include <vector>
#include <set>
using namespace std;

// Process bus transactions for MESI protocol
void processBusTransactions();

// Types of bus requests in MESI coherence protocol
enum class BusRequestType {
    READ_SHARED,        // Read request - others may have shared/modified copy
    READ_EXCLUSIVE,     // Read for write - others must invalidate
    UPGRADE_REQUEST     // Upgrade from shared to modified state
};

// Structure representing a bus transaction request
struct BusTransaction {
    int requestorId;            // ID of requesting processor
    int memoryAddress;          // Target memory address
    BusRequestType reqType;     // Type of bus request
};

// Structure for data transfer on bus
struct BusDataTransfer {
    int targetAddress;          // Memory address of cache line
    int destinationCore;        // ID of receiving processor
    bool isWriteOp;             // Read or write operation
    bool isWritebackOp;         // Writeback to memory flag
    bool isInvalidation;        // Invalidation signal
    int pendingCycles;          // Remaining cycles for transaction
};

extern vector<BusTransaction> pendingRequests;
extern vector<BusDataTransfer> dataTransferQueue;
#endif // BUS_HPP
