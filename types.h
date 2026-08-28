#pragma once

#include <cstdint>

/*
 * LPN: logical page number
 * PPN: physical page number (0, 1, 2, ...)
 * BlockId, PageOffset: split one block to multiple page
 *
 * PPN = BlockID * PagePerBlock + PageOffset
 */

using LPN = int32_t;
using PPN = int32_t;
using BlockId = int32_t;
using PageOffset = int32_t;

constexpr LPN INVALID_LPN = -1;
constexpr PPN INVALID_PPN = -1;
constexpr BlockId INVALID_BLOCK = -1;

// page owner information
enum class PageOwner: uint8_t {
    NONE, // right after erase
    USER, // exists user data
    MAP // For DRAM Cache(in NAND)
};

// mapping data for PPN -> LPN
struct PageTag {
    PageOwner owner = PageOwner::NONE;
    int32_t lpn = INVALID_LPN;
};
