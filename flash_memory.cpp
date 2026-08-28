#include "flash_memory.h"
#include <stdexcept>
#include <string>

// Block class constructor
Block::Block(int pages_per_block) {
    for (int i = 0; i < pages_per_block; i++) {
        pages.push_back(Page());
    }
}

// FlashMemory class constructor
FlashMemory::FlashMemory(int num_blocks, int pages_per_block) {
    numBlocks = num_blocks;
    pagesPerBlock = pages_per_block;

    for (int i = 0; i < num_blocks; i++) {
        blocks.push_back(Block(pages_per_block));
    }
}

// implement 3 functions
PageTag FlashMemory::read(PPN ppn) const {
    if (ppn < 0 || ppn >= totalPagesGet()) {
        throw out_of_range("invalid PPN " + to_string(ppn));
    }

    BlockId block_id = blockOf(ppn);
    PageOffset offset = offsetOf(ppn);

    Page page = blocks[block_id].pages[offset];

    // increase counter
    counters.reads++;

    // success(data exists)
    if (page.programmed) return page.tag;

    return PageTag();
}

void FlashMemory::program(PPN ppn, const PageTag& tag) {
    if (ppn < 0 || ppn >= totalPagesGet()) {
        throw out_of_range("invalid PPN " + to_string(ppn));
    }

    BlockId block_id = blockOf(ppn);
    PageOffset offset = offsetOf(ppn);

    // error when
    // 1) try programming already programmed position
    // 2) not sequential access(in block)
    if (blocks[block_id].pages[offset].programmed) {
        throw runtime_error("program failed: already programmed, PPN " + to_string(ppn));
    }

    if (offset > 0 && !blocks[block_id].pages[offset - 1].programmed) {
        throw runtime_error("program failed: it must be sequential access, PPN " + to_string(ppn));
    }

    // success program
    blocks[block_id].pages[offset].programmed = true;
    blocks[block_id].pages[offset].tag = tag;

    counters.programs++;
}

void FlashMemory::erase(BlockId blockId) {
    if (blockId < 0 || blockId >= numBlocksGet()) {
        throw out_of_range("invalid blockId " + to_string(blockId));
    }

    // traverse all pages in blockId and initialize them
    for (int i = 0; i < pagesPerBlock; i++) {
        blocks[blockId].pages[i].programmed = false;
        blocks[blockId].pages[i].tag = PageTag();
    }

    counters.erases++;
}
