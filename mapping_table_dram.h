#pragma once

#include <vector>
#include "types.h"

using namespace std;

/*
 * This file simulates dram : get LPN, returns PPN
 *
 * Thus, we only need to implement
 * 3 functions
 * 1) lookup (LPN -> PPN)
 * 2) update mapping table (LPN -> new PPN)
 * 3) remove (delete LPN -> PPN)
 *
 */

class MappingTable {
private:
    vector<PPN> table;

public:
    // 3 functions
    PPN lookup(LPN lpn) const;
    void update(LPN lpn, PPN ppn);
    void remove(LPN lpn);

    // constructor
    explicit MappingTable(int num_lpns);
};