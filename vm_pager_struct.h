#include <algorithm>
#include <cassert>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include "vm_arena.h"
#include "vm_pager.h"


// File Type
enum class file_t {
    SWAP,
    FILE_B,
};

// Physical Page State Bits
struct ppb {
    bool ref;     // reference bit
    bool dirty;   // dirty bit
};

// Page Table Info
struct Pt {
    uint32_t size;                              // TODO
    uint32_t numsw;                             // TODO
    std::unique_ptr<page_table_entry_t[]> st;   // TODO
};

// File Block Info
struct Infile {
    file_t ftype;           // file type
    bool infile;            // file location
    uint32_t block;         // file block number
    std::string filename;   // file name
};

// File-Back Page Info
struct fbp {
    uint32_t ppage;                        // physical page number
    std::set<page_table_entry_t*> vpset;   // set of mapped pte entries
};