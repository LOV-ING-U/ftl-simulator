#pragma once

#include "types.h"
#include "flash_memory.h"
#include "mapping_table_dram.h"

using namespace std;

/*
 * ftl controller file
 *
 * how to write data
 * 1) host(OS) requests "LPN 50 write"
 * 2) controller allocates appropriate position on NAND
 * 3) writes data on NAND, update mapping on DRAM
 *
 * OS only can request these functions:
 * 1) read(LPN)
 * 2) write(LPN)
 *
 * but FTL controller have to:
 * 1) program(write call)
 * 2) garbage collection(when triggers)
 * 3) handle override(already valid LPN -> PPN mapping and data exists)
 *
 */

// monitoring ftl state counters
struct FTLStates {
    uint64_t writeCallCount = 0;
    uint64_t gcRunCount = 0;
    uint64_t gcCopiedPagesCount = 0;
};

// simulate adjust factor
// 1. select victim block policy
enum class VictimPolicy {
    RANDOM,
    GREEDY,
    COST_BENEFIT
};

// 2. select new free block policy
enum class FreeBlockPolicy {
    ANY,
    DYNAMIC_WL
};

// ftl controller
class FTL {
private:
    // references
    FlashMemory& flash_memory; // NAND
    MappingTable mapping_table; // DRAM

    vector<int> validCount; // validCount[blockId] = valid page count
    vector<PageOffset> nextOffset; // nextOffset[blockId] = next offset when program
    vector<bool> isFree; // isFree[blockId] = this block is free(GC, Initial) or not
    BlockId openBlock; // currently using blockId
    int reservedFreeBlocks; // if reservedFreeBlocks >= emptyBlock, GC Triggers

    // functions for ftl controller
    PPN programPage(const PageTag& tag);

    bool garbageCollection();
    BlockId selectVictim() const;

    void invalidatePage(PPN ppn); // when already ppn filled, then -1 validCount of old ppn block (left org data, but cannot access)
    void openNewBlock(); // when openBlock is invalid(check at write attempt), pick free block and open

    // Policy
    VictimPolicy victimPolicy;

public:
    // constructor
    FTL (FlashMemory& flash, int numLPNs, int reserved, VictimPolicy vPolicy);

    // functions for OS
    PPN read(LPN lpn) const;
    void write(LPN lpn);
};
