#include "ftl_controller.h"
#include <stdexcept>
#include <string>
#include <random>

// constructor
FTL::FTL(FlashMemory& flash, int num_lpn, int reserved, VictimPolicy vPolicy, int endurance_limit): flash_memory(flash), mapping_table(num_lpn), victimPolicy(vPolicy){
    reservedFreeBlocks = reserved;
    openBlock = INVALID_BLOCK;
    enduranceLimit = endurance_limit;
    globalWriteClock = 0;

    for (int i = 0; i < flash.numBlocksGet(); i++) {
        validCount.push_back(0);
        nextOffset.push_back(0);
        eraseCount.push_back(0);
        isFree.push_back(true);
        blockClosedAt.push_back(0);
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
    if (nextOffset[openBlock] == flash_memory.pagesPerBlockGet()) {
        blockClosedAt[openBlock] = globalWriteClock;
        openBlock = INVALID_BLOCK;
    }

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
    } else { // victimPolicy == VictimPolicy::COST_BENEFIT
        BlockId victim = INVALID_BLOCK;

        // equation
        // v_p = validCount[blockId] / pagesPerBlockGet()
        // cost = 1 + v_p (1 = read all block / v_p = valid page should be written to other block
        // benefit = (1 - v_p) * age (1 - v_p = ratio of pages that can be erased without additional write / age = should be cleared as soon as possible)
        // score = benefit / cost

        double score_max = 0.0;

        for (int b = 0; b < flash_memory.numBlocksGet(); b++) {
            if (isFree[b] || b == openBlock) continue;
            if (eraseCount[b] >= enduranceLimit) continue;

            double v_p = (double)validCount[b] / flash_memory.pagesPerBlockGet();
            int age = globalWriteClock - blockClosedAt[b];
            double score = (1.0 - v_p) * (double)age / (1.0 + v_p);

            if (score > score_max) {
                score_max = score;
                victim = b;
            }
        }

        return victim;
    }
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
    // global clock
    globalWriteClock++;

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

int FTL::maxEraseCountGet() const {
    int m = eraseCount[0];
    for (int v : eraseCount) if (v > m) m = v;
    return m;
}

int FTL::minEraseCountGet() const {
    int m = eraseCount[0];
    for (int v : eraseCount) if (v < m) m = v;
    return m;
}

int FTL::retiredBlockCountGet() const {
    int c = 0;
    for (int v : eraseCount) if (v >= enduranceLimit) c++;
    return c;
}
