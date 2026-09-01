#include "ftl_controller.h"
#include <stdexcept>
#include <string>
#include <random>

// constructor
FTL::FTL(FlashMemory& flash, int num_lpn, int reserved, VictimPolicy vPolicy, int endurance_limit): flash_memory(flash), mapping_table(num_lpn), victimPolicy(vPolicy){
    reservedFreeBlocks = reserved;
    openBlock = INVALID_BLOCK;
    enduranceLimit = endurance_limit;

    for (int i = 0; i < flash.numBlocksGet(); i++) {
        validCount.push_back(0);
        nextOffset.push_back(0);
        isFree.push_back(true);
    }
}

// open front free block
void FTL::openNewBlock() {
    for (int b = 0; b < flash_memory.numBlocksGet(); b++) {
        if (isFree[b]) {
            openBlock = b;
            isFree[b] = false;
            nextOffset[b] = 0;
            return;
        }
    }

    throw runtime_error("openNewBlock failed: no free block");
}

// program tag on openBlock
PPN FTL::programPage(const PageTag& tag) {
    if (openBlock == INVALID_BLOCK) openNewBlock();

    PPN ppn = flash_memory.makePPN(openBlock, nextOffset[openBlock]);

    flash_memory.program(ppn, tag);
    nextOffset[openBlock]++;
    validCount[openBlock]++;

    // this block is full
    if (nextOffset[openBlock] == flash_memory.pagesPerBlockGet()) openBlock = INVALID_BLOCK;

    return ppn;
}

// count -1 of valid data count on blockOf(ppn)
void FTL::invalidatePage(PPN ppn) {
    BlockId b_id = flash_memory.blockOf(ppn);

    if (validCount[b_id] > 0) validCount[b_id]--;
}

// select victim blockId
BlockId FTL::selectVictim() const {
    // 1. random
    if (victimPolicy == VictimPolicy::RANDOM) {
        static mt19937 rng(123);
        vector<BlockId> candidates;

        // just choose random victim
        for (int b = 0; b < flash_memory.numBlocksGet(); b++) {
            if (isFree[b] || b == openBlock) continue;
            if (eraseCount[b] >= enduranceLimit) continue;

            candidates.push_back(b);
        }

        if (candidates.empty()) return INVALID_BLOCK;

        // pick random
        uniform_int_distribution<int> pick(0, (int)candidates.size() - 1);

        return candidates[pick(rng)];
    } else if (victimPolicy == VictimPolicy::GREEDY) {
        BlockId victim = INVALID_BLOCK;
        int invalid_count = 0;

        for (int b = 0; b < flash_memory.numBlocksGet(); b++) {
            if (isFree[b] || b == openBlock) continue;
            if (eraseCount[b] >= enduranceLimit) continue;

            int invalid_b = flash_memory.pagesPerBlockGet() - validCount[b];

            if (invalid_b > invalid_count) {
                invalid_count = invalid_b;
                victim = b;
            }
        }

        return victim;
    }

    return 0;
}

// gc
// clear victim block(move valid data to other block)
bool FTL::garbageCollection() {
    BlockId victim = selectVictim();
    if (victim == INVALID_BLOCK) return false;

    // clear
    for (int offset = 0; offset < flash_memory.pagesPerBlockGet(); offset++) {
        PPN ppn = flash_memory.makePPN(victim, offset);
        PageTag tag = flash_memory.read(ppn);

        if (tag.owner != PageOwner::USER) continue;

        if (mapping_table.lookup(tag.lpn) != ppn) continue;

        PPN new_ppn = programPage(tag);
        mapping_table.update(tag.lpn, new_ppn);
    }

    flash_memory.erase(victim);
    eraseCount[victim]++;
    validCount[victim] = 0;
    isFree[victim] = true;
    return true;
}

// read data
PPN FTL::read(LPN lpn) const {
    PPN ppn = mapping_table.lookup(lpn);

    // if existing ppn
    if (ppn != INVALID_PPN) flash_memory.read(ppn);

    return ppn;
}

void FTL::write(LPN lpn) {
    // gc triggers only openBlock == INVALID
    while (openBlock == INVALID_BLOCK) {
        int freeBlock_count = 0;
        for (int b = 0; b < flash_memory.numBlocksGet(); b++) {
            if (isFree[b]) freeBlock_count++;
        }

        if (freeBlock_count >= reservedFreeBlocks) break;

        if (!garbageCollection()) break;
    }

    PPN old_ppn = mapping_table.lookup(lpn);
    PPN new_ppn = programPage(PageTag{.owner = PageOwner::USER, .lpn = lpn});

    // if mapping already exists
    if (old_ppn != INVALID_PPN) invalidatePage(old_ppn);

    mapping_table.update(lpn, new_ppn);
}
