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

double computeHostIOPS(uint64_t programs, uint64_t erases, uint64_t reads, uint64_t hostOps, double programLatencyMs, double eraseLatencyMs, double readLatencyMs) {
    double totalMs = programs * programLatencyMs + erases * eraseLatencyMs + reads * readLatencyMs;
    double totalSec = totalMs / 1000.0;
    return (double)hostOps / totalSec;
}

void runSimulate(int numBlocks, int pagesPerBlock, int num_lpns, int reservedFreeBlocks, VictimPolicy victimPolicy, WritePattern writePattern, double zipfS, int enduranceLimit, int index) {
    FlashMemory flash(numBlocks, pagesPerBlock);
    FTL ftl_controller(flash, num_lpns, reservedFreeBlocks, victimPolicy, enduranceLimit);

    mt19937 rng(42);
    uniform_int_distribution<int> uDist(0, num_lpns - 1);

    vector<double> weights;
    if (writePattern == WritePattern::ZIPF_DISTRIBUTE) {
        weights.resize(num_lpns);
        for (int k = 0; k < num_lpns; k++) weights[k] = 1.0 / pow(k + 1, zipfS);
    }

    discrete_distribution<int> zDist(weights.begin(), weights.end());

    uint64_t survivedWrites = 0;
    bool firstRetireFound = false;
    uint64_t firstRetireWrites = 0, firstRetireProgram = 0, firstRetireErase = 0, firstRetireRead = 0;

    try {
        while (true) {
            int lpn = (writePattern == WritePattern::UNIFORM) ? uDist(rng) : zDist(rng);
            ftl_controller.write(lpn);
            survivedWrites++;

            if (!firstRetireFound && ftl_controller.retiredBlockCountGet() >= 1) {
                firstRetireFound = true;
                firstRetireWrites = survivedWrites;
                firstRetireProgram = flash.countersGet().programs;
                firstRetireErase = flash.countersGet().erases;
                firstRetireRead = flash.countersGet().reads;
            }
        }
    } catch (const exception& e) {
        // ssd die (cannot write anymore)
    }

    if (!firstRetireFound) {
        firstRetireWrites = survivedWrites;
        firstRetireProgram = flash.countersGet().programs;
        firstRetireErase = flash.countersGet().erases;
        firstRetireRead = flash.countersGet().reads;
    }

    uint64_t totalProgram = flash.countersGet().programs;
    uint64_t totalErase = flash.countersGet().erases;
    uint64_t totalRead = flash.countersGet().reads;

    uint64_t p1Writes = firstRetireWrites;
    double p1Waf = (p1Writes > 0) ? (double)firstRetireProgram / p1Writes : 0.0;
    double p1Iops = (p1Writes > 0) ? computeHostIOPS(firstRetireProgram, firstRetireErase, firstRetireRead, p1Writes, 1.4, 12.5, 0.1) : 0.0;

    uint64_t p2Writes = survivedWrites - firstRetireWrites;
    uint64_t p2Program = totalProgram - firstRetireProgram;
    uint64_t p2Erase = totalErase - firstRetireErase;
    uint64_t p2Read = totalRead - firstRetireRead;
    double p2Waf = (p2Writes > 0) ? (double)p2Program / p2Writes : 0.0;
    double p2Iops = (p2Writes > 0) ? computeHostIOPS(p2Program, p2Erase, p2Read, p2Writes, 1.4, 12.5, 0.1) : 0.0;

    int totalPages = numBlocks * pagesPerBlock;
    double utilization = (double)num_lpns / totalPages;

    printf("%d. utilization %.0f%% (LPN %d / PPN %d)\n", index, utilization * 100, num_lpns, totalPages);
    printf("   [window 1: write 0 ~ first block die(%llu)]     writes=%llu  WAF=%.3f  IOPS=%.0f\n", (unsigned long long)firstRetireWrites, (unsigned long long)p1Writes, p1Waf, p1Iops);
    printf("   [window 2: first block die ~ ssd die(%llu)]   writes=%llu  WAF=%.3f  IOPS=%.0f\n", (unsigned long long)survivedWrites, (unsigned long long)p2Writes, p2Waf, p2Iops);
    printf("   total survived writes : %llu\n", (unsigned long long)survivedWrites);
    printf("   retired blocks        : %d / %d\n\n", ftl_controller.retiredBlockCountGet(), numBlocks);
}

void runExperiment(int expNum, VictimPolicy victimPolicy, const char* victimLabel, WritePattern writePattern, const char* writePatternLabel, double zipfS, int numBlocks, int pagesPerBlock, int reservedFreeBlocks, int enduranceLimit, const double* utilizations, int utilizationCount) {
    if (writePattern == WritePattern::ZIPF_DISTRIBUTE) {
        printf("[Experiment %d] Write Pattern = %s (s = %.1f) / VictimPolicy = %s / Next free block choose policy = ANY / endurance limit = %d\n\n", expNum, writePatternLabel, zipfS, victimLabel, enduranceLimit);
    } else {
        printf("[Experiment %d] Write Pattern = %s / VictimPolicy = %s / Next free block choose policy = ANY / endurance limit = %d\n\n", expNum, writePatternLabel, victimLabel, enduranceLimit);
    }

    int totalPages = numBlocks * pagesPerBlock;
    for (int i = 0; i < utilizationCount; i++) {
        int num_lpn = (int)(totalPages * utilizations[i]);
        runSimulate(numBlocks, pagesPerBlock, num_lpn, reservedFreeBlocks, victimPolicy, writePattern, zipfS, enduranceLimit, i + 1);
    }
    printf("\n");
}

int main() {
    int numBlocks = 64;
    int pagesPerBlock = 64;
    int reservedFreeBlocks = 2;
    int enduranceLimit = 300;
    double utilizations[] = {0.50, 0.70, 0.85, 0.90};
    int utilizationCount = 4;

    runExperiment(1, VictimPolicy::RANDOM, "RANDOM", WritePattern::UNIFORM, "UNIFORM", 0.0, numBlocks, pagesPerBlock, reservedFreeBlocks, enduranceLimit, utilizations, utilizationCount);
    runExperiment(2, VictimPolicy::GREEDY, "GREEDY", WritePattern::UNIFORM, "UNIFORM", 0.0, numBlocks, pagesPerBlock, reservedFreeBlocks, enduranceLimit, utilizations, utilizationCount);
    runExperiment(3, VictimPolicy::GREEDY, "GREEDY", WritePattern::ZIPF_DISTRIBUTE, "ZIPF", 1.2, numBlocks, pagesPerBlock, reservedFreeBlocks, enduranceLimit, utilizations, utilizationCount);
    runExperiment(4, VictimPolicy::COST_BENEFIT, "COST_BENEFIT", WritePattern::ZIPF_DISTRIBUTE, "ZIPF", 1.2, numBlocks, pagesPerBlock, reservedFreeBlocks, enduranceLimit, utilizations, utilizationCount);
}