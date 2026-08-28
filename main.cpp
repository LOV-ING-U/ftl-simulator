#include <iostream>

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