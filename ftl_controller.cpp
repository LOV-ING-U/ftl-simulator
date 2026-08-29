#pragma once

#include "ftl_controller.h"
#include <stdexcept>
#include <string>

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

    throw runtine_error("openNewBlock failed: no free block");
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
    BlockId victim = INVALID_BLOCK;
    int invalid_count = 0;

    for (int b = 0; b < flash_memory.numBlocksGet(); b++) {
        if (isFree[b] || b == openBlock) continue;

        int invalid_b = flash_memory.pagesPerBlockGet() - validCount[b];

        if (invalid_b > invalid_count) {
            invalid_count = invalid_b;
            victim = b;
        }
    }

    return victim;
}

// gc
bool FTL::garbageCollection() {

}
