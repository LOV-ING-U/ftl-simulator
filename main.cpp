#include <iostream>
#include <random>
#include <vector>
#include <cmath>
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

enum class WritePattern {
    UNIFORM,
    ZIPF_DISTRIBUTE
};

double computeHostIOPS(uint64_t programs, uint64_t erases, uint64_t reads, int hostOps, double programLatencyMs, double eraseLatencyMs, double readLatencyMs) {
    double totalMs = programs * programLatencyMs + erases * eraseLatencyMs + reads * readLatencyMs;
    double totalSec = totalMs / 1000.0;
    return hostOps / totalSec;
}

void runSimulate(int numBlocks, int pagesPerBlock, int num_lpns, int reservedFreeBlocks, int writeCount, VictimPolicy victimPolicy, WritePattern writePattern, double zipfS, int index) {
    FlashMemory flash(numBlocks, pagesPerBlock);
    FTL ftl_controller(flash, num_lpns, reservedFreeBlocks, victimPolicy);

    mt19937 rng(42);

    if (writePattern == WritePattern::UNIFORM) {
        uniform_int_distribution<int> dist(0, num_lpns - 1);
        for (int i = 0; i < writeCount; i++) {
            ftl_controller.write(dist(rng));
        }
    } else {
        vector<double> weights(num_lpns);
        for (int k = 0; k < num_lpns; k++) {
            weights[k] = 1.0 / pow(k + 1, zipfS);
        }
        discrete_distribution<int> dist(weights.begin(), weights.end());
        for (int i = 0; i < writeCount; i++) {
            ftl_controller.write(dist(rng));
        }
    }

    int totalPages = numBlocks * pagesPerBlock;
    double utilization = (double)num_lpns / totalPages;
    double waf = (double)flash.countersGet().programs / writeCount;
    double iops = computeHostIOPS(flash.countersGet().programs, flash.countersGet().erases, flash.countersGet().reads, writeCount, 1.4, 12.5, 0.1);

    printf("%d. utilization %.0f%% (LPN %d / PPN %d)\n", index, utilization * 100, num_lpns, totalPages);
    printf("   host writes   : %d\n", writeCount);
    printf("   flash programs: %llu\n", (unsigned long long)flash.countersGet().programs);
    printf("   flash erases  : %llu\n", (unsigned long long)flash.countersGet().erases);
    printf("   WAF           : %.3f\n", waf);
    printf("   host IOPS     : %.0f\n\n", iops);
}

void runExperiment(int expNum, VictimPolicy victimPolicy, const char* victimLabel, WritePattern writePattern, const char* writePatternLabel, double zipfS, int numBlocks, int pagesPerBlock, int reservedFreeBlocks, int writeCount, const double* utilizations, int utilizationCount) {
    if (writePattern == WritePattern::ZIPF_DISTRIBUTE) {
        printf("[Experiment %d] Write Pattern = %s(s=%.1f) / VictimPolicy = %s / Next free block choose policy = ANY\n\n", expNum, writePatternLabel, zipfS, victimLabel);
    } else {
        printf("[Experiment %d] Write Pattern = %s / VictimPolicy = %s / Next free block choose policy = ANY\n\n", expNum, writePatternLabel, victimLabel);
    }

    int totalPages = numBlocks * pagesPerBlock;
    for (int i = 0; i < utilizationCount; i++) {
        int num_lpn = (int)(totalPages * utilizations[i]);

        runSimulate(numBlocks, pagesPerBlock, num_lpn, reservedFreeBlocks, writeCount, victimPolicy, writePattern, zipfS, i + 1);
    }
    printf("\n");
}

int main() {
    int numBlocks = 64;
    int pagesPerBlock = 64;
    int reservedFreeBlocks = 2;
    int writeCount = 200000;
    double utilizations[] = {0.50, 0.70, 0.85, 0.90};
    int utilizationCount = 4;

    runExperiment(1, VictimPolicy::RANDOM, "RANDOM", WritePattern::UNIFORM, "UNIFORM", 0.0, numBlocks, pagesPerBlock, reservedFreeBlocks, writeCount, utilizations, utilizationCount);

    runExperiment(2, VictimPolicy::GREEDY, "GREEDY", WritePattern::UNIFORM, "UNIFORM", 0.0, numBlocks, pagesPerBlock, reservedFreeBlocks, writeCount, utilizations, utilizationCount);

    runExperiment(3, VictimPolicy::GREEDY, "GREEDY", WritePattern::ZIPF_DISTRIBUTE, "ZIPF", 1.2, numBlocks, pagesPerBlock, reservedFreeBlocks, writeCount, utilizations, utilizationCount);
}
