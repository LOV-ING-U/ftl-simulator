#include <iostream>
#include <random>
#include "flash_memory.h"
#include "ftl_controller.h"

/*
simple FTL Simulator

SSD = multiple Blocks
Block = multiple Pages

Read: per Page
Write: per Page
Erase: per Block

+ Once some page written by data, that page MUST BE erased by 0 to write new data.

-- Physical Hierarchy of SSD
1) DRAM: Store Mapping Table
2) NAND: Store Real Data
3) Controller: Run FTL
*/

using namespace std;

void runSimulateUniform(int numBlocks, int pagesPerBlock, int num_lpns, int reservedFreeBlocks, int writeCount) {
    FlashMemory flash(numBlocks, pagesPerBlock);
    FTL ftl_controller(flash, num_lpns, reservedFreeBlocks);

    mt19937 rng(42);
    uniform_int_distribution<int> dist(0, num_lpns - 1);

    for (int i = 0; i < writeCount; i++) {
        ftl_controller.write(dist(rng));
    }

    int totalPages = numBlocks * pagesPerBlock;
    double utilization = (double)num_lpns / totalPages;
    double waf = (double)flash.countersGet().programs / writeCount;

    printf("utilization %.0f%% (LPN %d / PPN %d)\n", utilization * 100, num_lpns, totalPages);
    printf("  host writes   : %d\n", writeCount);
    printf("  flash programs: %llu\n", (unsigned long long)flash.countersGet().programs);
    printf("  flash erases  : %llu\n", (unsigned long long)flash.countersGet().erases);
    printf("  WAF           : %.3f\n\n", waf);
}

int main() {
    int numBlocks = 64;
    int pagesPerBlock = 64;
    int reservedFreeBlocks = 2;
    int writeCount = 200000;
    int totalPages = numBlocks * pagesPerBlock;

    double utilizations[] = {0.50, 0.70, 0.85, 0.90};
    for (double u : utilizations) {
        int num_lpn = totalPages * u;
        runSimulateUniform(numBlocks, pagesPerBlock, num_lpn, reservedFreeBlocks, writeCount);
    }
}