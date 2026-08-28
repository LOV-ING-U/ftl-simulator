#include "mapping_table_dram.h"
#include <stdexcept>
#include <string>

MappingTable::MappingTable(int num_lpns) {
    for (int i = 0; i < num_lpns; i++) {
        table.push_back(INVALID_PPN);
    }
}

// 3 functions
PPN MappingTable::lookup(LPN lpn) const {
    if (lpn < 0 || lpn >= table.size()) {
        throw out_of_range("invalid LPN " + to_string(lpn));
    }

    return table[lpn];
}

void MappingTable::update(LPN lpn, PPN ppn) {
    if (lpn < 0 || lpn >= table.size()) {
        throw out_of_range("invalid LPN " + to_string(lpn));
    }

    table[lpn] = ppn;
}

void MappingTable::remove(LPN lpn) {
    if (lpn < 0 || lpn >= table.size()) {
        throw out_of_range("invalid LPN " + to_string(lpn));
    }

    table[lpn] = INVALID_PPN;
}
