#pragma once

#include <vector>
#include "types.h"

using namespace std;

/*
 * this header/cpp file simulates Flash Memory(NAND)
 *
 * only gives 3 functions
 * 1) read
 * 2) program
 * 3) erase
 */

struct Page {
    bool programmed = false; // can overwrite or not
    PageTag tag;
};

struct Block {
    vector<Page> pages;

    explicit Block(int pagesPerBlock);
};

struct Counters {
    uint64_t reads = 0;
    uint64_t programs = 0;
    uint64_t erases = 0;
};

// real flash memory
class FlashMemory {
private:
    int numBlocks;
    int pagesPerBlock;
    vector<Block> blocks; // data
    mutable Counters counters; // read, program, erase counter

public:
    // first initialize NAND
    FlashMemory(int numBlocks, int pagesPerBlock);

    // only gives 3 function
    PageTag read(PPN ppn) const; // now, just give pagetag instead real data
    void program(PPN ppn, const PageTag& tag); // write "once"
    void erase(BlockId blockId); // erase contents of one block

    // get fields
    int numBlocksGet() const {
        return numBlocks;
    }

    int pagesPerBlockGet() const {
        return pagesPerBlock;
    }

    int totalPagesGet() const {
        return numBlocks * pagesPerBlock;
    }

    const Counters& countersGet() const {
        return counters;
    }

    // calculate address
    BlockId blockOf(PPN ppn) const {
        return ppn / pagesPerBlock;
    }

    PageOffset offsetOf(PPN ppn) const {
        return ppn % pagesPerBlock;
    }

    PPN makePPN(BlockId blockId, PageOffset offset) const {
        return blockId * pagesPerBlock + offset;
    }
};
