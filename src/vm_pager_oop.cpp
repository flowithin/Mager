#include "vm_pager.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>

#include "vm_arena.h"

// #define LOG

enum class MEM_STATUS { FREE, REFERENCED, PEND, PINNED, GHOST_REF, GHOST_PEND };   // PEND: can be evicted
enum class DB_STATUS { CLEAN, DIRTY };
enum class SWAP_STATUS { AVAILABLE, ALLOCATED };
enum class RESIDENCE { NONE, PHYS, DISK };

struct page_block_entry_t {
    unsigned int block;
    std::string filename;

    page_block_entry_t()
        : block(VM_ARENA_SIZE / VM_PAGESIZE)
        , filename("NULL") {}
};

// CPU Information
pid_t running_pid;

// Process Information
static constexpr unsigned int VM_PAGENUM = VM_ARENA_SIZE / VM_PAGESIZE;
std::unordered_map<pid_t, page_table_entry_t*> ProcessPageTables;   // Pointers to each process's page table
std::unordered_map<pid_t, page_block_entry_t*> ProcessPageBlocks;   // Pointers to each process's block table
std::unordered_map<pid_t, unsigned int> ProcessNextVPage;   // The number of the process's next available virtual page

// Physical Memory States
struct phys_mem_entry_t {
    MEM_STATUS state;
    DB_STATUS dirty_bit;
    std::unique_ptr<page_block_entry_t> write_back_loc;

    phys_mem_entry_t()
        : state(MEM_STATUS::FREE)
        , dirty_bit(DB_STATUS::CLEAN)
        , write_back_loc(nullptr) {}
};
std::vector<phys_mem_entry_t> PhysMem;   // Physical Mem Occupancy
std::queue<size_t> MemQueue;             // Queue for implementing the clock algorithm
std::queue<size_t> FreePhys;

// Swap Block States
std::vector<SWAP_STATUS> SwapBlocks;
size_t FirstFreeSwap;

// File Block States
// std::unordered_map<std::pair<const void*, unsigned int>, std::set<page_table_entry_t*>> MappedBlocks;
std::unordered_map<std::string, std::unordered_map<unsigned int, std::set<page_table_entry_t*>>> MappedBlocks;

// Helper Functions
inline void* getArenaPageAddress(unsigned int vp) {
    return static_cast<void*>(static_cast<char*>(VM_ARENA_BASEADDR) + vp * VM_PAGESIZE);
}

inline unsigned int getArenaPageNumber(const void* address) {
    return static_cast<unsigned int>(static_cast<const char*>(address) - static_cast<char*>(VM_ARENA_BASEADDR))
         / VM_PAGESIZE;
}

inline bool addressInArena(const void* address) {
    return (reinterpret_cast<uintptr_t>(address) >= reinterpret_cast<uintptr_t>(VM_ARENA_BASEADDR)
            && reinterpret_cast<uintptr_t>(address) <= reinterpret_cast<uintptr_t>(VM_ARENA_BASEADDR) + VM_ARENA_SIZE);
}

// Debug Functions
void printphysical() {
#ifdef LOG
    std::cout << "Physical Memory State" << std::endl;
    for (size_t i = 0; i < PhysMem.size(); ++i) {
        std::cout << i << " | " << PhysMem[i].dirty_bit << " | " << PhysMem[i].state << std::endl;
    }
    std::cout << "------" << std::endl;
#endif
}

void printdebug() {
#ifdef LOG
    /* Debug Block Info */
    for (auto& fileBlocks : MappedBlocks) {
        std::cout << "Filename: " << fileBlocks.first << std::endl;
        for (auto& block : fileBlocks.second) {
            std::cout << "Block Number: " << block.first << "| Mapped Size: " << block.second.size() << std::endl;
            std::cout << "Mapped ppage: " << (*block.second.begin())->ppage << " is read "
                      << (*block.second.begin())->read_enable << " and write " << (*block.second.begin())->write_enable
                      << std::endl;
        }
        std::cout << "---" << std::endl;
    }
#endif
}

void printAllPageBlocks() {
#ifdef LOG
    for (const auto& pair : ProcessPageBlocks) {
        pid_t pid = pair.first;
        page_block_entry_t* blocks = pair.second;

        std::cout << "PID: " << pid << " at address " << reinterpret_cast<uintptr_t>(blocks) << "\n";
        for (size_t i = 0; i < VM_PAGENUM; ++i) {
            if (blocks[i].filename == "OOPS") {
                break;
            }
            std::cout << "  Block " << i << " | block #: " << blocks[i].block << " | filename: " << blocks[i].filename
                      << std::endl;
        }
    }
#endif
}

/* VM_INIT START */
void vm_init(unsigned int memory_pages, unsigned int swap_blocks) {
    /* initialize the physical mem */
    PhysMem.resize(memory_pages);

    // pin the first page
    PhysMem[0].state = MEM_STATUS::PINNED;
    PhysMem[0].dirty_bit = DB_STATUS::CLEAN;
    PhysMem[0].write_back_loc = nullptr;
    // fill the first page with zero
    char* pinnedpage = static_cast<char*>(vm_physmem);
    memset(pinnedpage, 0, VM_PAGESIZE);

    // push other pages into the queue
    for (size_t mem_page = 1; mem_page < memory_pages; ++mem_page) {
        FreePhys.push(mem_page);
    }

    /* initialize the swap blocks*/
    // initialize all as free
    SwapBlocks.resize(swap_blocks, SWAP_STATUS::AVAILABLE);
    // set the first available swap block to be 0
    FirstFreeSwap = 0;

    /* initialize running pid*/
    running_pid = -1;
}
/* VM_INIT END */

/* VM_CREATE START */
int vm_create(pid_t parent_pid, pid_t child_pid) {
    // NOTE: considers only calls by processes with an empty arena

    // initialize the page table
    ProcessPageTables[child_pid] = new page_table_entry_t[VM_PAGENUM];
    for (size_t i = 0; i < VM_PAGENUM; ++i) {
        ProcessPageTables[child_pid][i].ppage = PhysMem.size();
        ProcessPageTables[child_pid][i].read_enable = 0;
        ProcessPageTables[child_pid][i].write_enable = 0;
    }
    // next available page is page 0
    ProcessNextVPage[child_pid] = 0;

    // initialize the block table
    ProcessPageBlocks[child_pid] = new page_block_entry_t[VM_PAGENUM];

    // success
    return 0;
}
/* VM_CREATE END */

/* VM_SWITCH START */
void vm_switch(pid_t pid) {
    // point to the new process's page table
    page_table_base_register = ProcessPageTables[pid];
    // record the new pid
    running_pid = pid;
}
/* VM_SWITCH END */

/* VM_FAULT START */
void move_to_disk(page_table_entry_t* pte) {
    pte->read_enable = 0;
    pte->write_enable = 0;
    pte->ppage = PhysMem.size();
}

void reset_phys_page(unsigned int pmem) {
    PhysMem[pmem].state = MEM_STATUS::FREE;
    PhysMem[pmem].dirty_bit = DB_STATUS::CLEAN;
    PhysMem[pmem].write_back_loc.reset();
}

void assert_private_swap_block(std::set<page_table_entry_t*>& pte_set) {
    assert(pte_set.size() == 1);
}

std::set<page_table_entry_t*>& get_mapped_pte_set(unsigned int ppage) {
    std::string write_back_file = PhysMem[ppage].write_back_loc->filename;
    unsigned int write_back_block = PhysMem[ppage].write_back_loc->block;
    std::set<page_table_entry_t*>& pte_set = MappedBlocks[write_back_file][write_back_block];
    if (write_back_file == "SWAP") {
        assert_private_swap_block(pte_set);   // swap files are private
    }
    return pte_set;
}

// get the next physical page and return the ppage number
int get_ppage() {
    size_t pmem;
    void* phy_buff;

    // use the free page directly if possible
    if (!FreePhys.empty()) {
        pmem = FreePhys.front();
        FreePhys.pop();
        // MemQueue.push(pmem);

        return pmem;
    }

    // loop through the physical mem queue until an eviction/move happens
    while (true) {
        // move the top page to the tail
        pmem = MemQueue.front();
        assert(PhysMem[pmem].state != MEM_STATUS::FREE);
        MemQueue.pop();

        // find the according physical address
        phy_buff = static_cast<char*>(vm_physmem) + pmem * VM_PAGESIZE;

        // evict the pending page
        if (PhysMem[pmem].state == MEM_STATUS::PEND) {
            /* evict the old page */
            const char* write_back_file = PhysMem[pmem].write_back_loc->filename.c_str();
            if (PhysMem[pmem].write_back_loc->filename == "SWAP") {
                write_back_file = nullptr;
            }
            unsigned int write_back_block = PhysMem[pmem].write_back_loc->block;

            // write back ONLY IF DIRTY
            if (PhysMem[pmem].dirty_bit == DB_STATUS::DIRTY) {
                // std::cout << "Writing back now" << std::endl;
                if (file_write(write_back_file, write_back_block, phy_buff) == -1) {
                    return -1;
                };
            }

            // mark all mapped ptes as on disk
            std::set<page_table_entry_t*>& pte_set = get_mapped_pte_set(pmem);
            for (auto& pte : pte_set) {
                move_to_disk(pte);
            }

            // mark the physical page as free
            reset_phys_page(pmem);

            // return the page number
            return pmem;
        }

        // evict the ghost page
        if (PhysMem[pmem].state == MEM_STATUS::GHOST_PEND) {
            // std::cout << "Evicting Ghost Page" << std::endl;
            // get the info
            std::string filename = PhysMem[pmem].write_back_loc->filename;
            unsigned int block = PhysMem[pmem].write_back_loc->block;
            std::set<page_table_entry_t*>& pte_set = MappedBlocks[filename][block];

            // write back at if dirty
            if (PhysMem[pmem].dirty_bit == DB_STATUS::DIRTY) {
                assert(file_write(filename.c_str(), block, static_cast<char*>(vm_physmem) + pmem * VM_PAGESIZE) == 0);
            }

            // clean the mapped blocks
            assert(pte_set.size() == 1);
            delete *pte_set.begin();
            pte_set.clear();
            assert(pte_set.size() == 0);
            // remove the dict entry
            MappedBlocks[filename].erase(block);
            if (MappedBlocks[filename].size() == 0) {
                MappedBlocks.erase(filename);
            }

            // mark the physical page as free
            reset_phys_page(pmem);

            // return the page number
            return pmem;
        }

        // mark referenced page as pend
        if (PhysMem[pmem].state == MEM_STATUS::REFERENCED) {
            PhysMem[pmem].state = MEM_STATUS::PEND;

            // mark as on disk
            std::set<page_table_entry_t*>& pte_set = get_mapped_pte_set(pmem);

            // mark as r = 0 and w = 0 WITHOUT resetting the ppage so that next time it can be marked as REFERENCED
            for (auto& pte : pte_set) {
                pte->read_enable = 0;
                pte->write_enable = 0;
            }
        }

        // mark ghost_ref as ghost_pend
        if (PhysMem[pmem].state == MEM_STATUS::GHOST_REF) {
            PhysMem[pmem].state = MEM_STATUS::GHOST_PEND;

            // mark as on disk
            std::set<page_table_entry_t*>& pte_set = get_mapped_pte_set(pmem);

            // mark as r = 0 and w = 0 WITHOUT resetting the ppage so that next time it can be marked as REFERENCED
            for (auto& pte : pte_set) {
                pte->read_enable = 0;
                pte->write_enable = 0;
            }
        }

        MemQueue.push(pmem);
    }
}

int vm_fault(const void* addr, bool write_flag) {
    printphysical();
    // check if the address is in the arena
    if (!addressInArena(addr)) {
        return -1;
    }

    // check if the address is valid
    if (getArenaPageNumber(addr) >= ProcessNextVPage[running_pid]) {
        return -1;
    }

    // assert vm_switch has been called
    assert(running_pid != -1);

    // get the virtual page number of fault
    unsigned int vp = getArenaPageNumber(addr);

    // get the according PTE & BTE
    page_table_entry_t* pte = ProcessPageTables[running_pid] + vp;
    page_block_entry_t* bte = ProcessPageBlocks[running_pid] + vp;

    bool write_enabled = pte->write_enable;
    bool read_enabled = pte->read_enable;
    bool is_file_back = (bte->filename != "SWAP");

    // std::cout << "VP" << vp << " is write " << write_enabled << " and read " << read_enabled << " with mapped ppage "
    // << pte->ppage << std::endl;

    // on physical memory & readable & referenced
    if (write_enabled == 0 && read_enabled == 1) {
        // find the mapped physical page
        unsigned int mapped_ppage = ProcessPageTables[running_pid][vp].ppage;
        // std::cout << PhysMem[mapped_ppage].state << std::endl;
        assert(PhysMem[mapped_ppage].state == MEM_STATUS::REFERENCED
               || PhysMem[mapped_ppage].state == MEM_STATUS::PINNED);
        assert(mapped_ppage != PhysMem.size());
        // file-back pages or swap-back pages not mapped to pinned
        if (is_file_back || mapped_ppage != 0) {
            // find all page table entries mapped to the physical page
            std::set<page_table_entry_t*>& pte_set = get_mapped_pte_set(mapped_ppage);
            // mark as write enabled
            for (auto& pte : pte_set) {
                pte->write_enable = 1;
            }
            // mark dirty bit
            PhysMem[mapped_ppage].dirty_bit = DB_STATUS::DIRTY;
        }
        // swap-back pages that are mapped to pinned
        else {
            /* copy-on-write */
            int free_pmem = get_ppage();
            // catch error in case write back fails
            if (free_pmem == -1) {
                return -1;
            }
            MemQueue.push(free_pmem);
            // mark dirty bit
            PhysMem[free_pmem].dirty_bit = DB_STATUS::DIRTY;
            // set up the physical memory block
            PhysMem[free_pmem].state = MEM_STATUS::REFERENCED;
            PhysMem[free_pmem].write_back_loc = std::make_unique<page_block_entry_t>(*bte);
            // find the according physical address
            void* phy_buff = static_cast<char*>(vm_physmem) + free_pmem * VM_PAGESIZE;
            // copy to the page
            memcpy(phy_buff, vm_physmem, VM_PAGESIZE);

            // update page mapping and mark as write enabled
            std::set<page_table_entry_t*>& pte_set = get_mapped_pte_set(free_pmem);
            assert(pte_set.size() == 1);
            for (auto& pte : pte_set) {
                pte->ppage = free_pmem;
                pte->write_enable = 1;
            }
        }
    }

    // on physical memory & pending
    if (write_enabled == 0 && read_enabled == 0 && pte->ppage != PhysMem.size()) {
        unsigned int pmem = pte->ppage;
        // mark as referenced
        PhysMem[pmem].state = MEM_STATUS::REFERENCED;
        unsigned int re;
        unsigned int we;
        // if is dirty, mark as readable & writable
        if (PhysMem[pmem].dirty_bit == DB_STATUS::DIRTY) {
            re = 1;
            we = 1;
        } else {
            if (write_flag) {
                PhysMem[pmem].dirty_bit = DB_STATUS::DIRTY;
                re = 1;
                we = 1;
            } else {
                re = 1;
                we = 0;
            }
        }

        // mark all relevant entries
        std::set<page_table_entry_t*>& pte_set = get_mapped_pte_set(pmem);
        for (auto& pte : pte_set) {
            pte->read_enable = re;
            pte->write_enable = we;
        }
    }

    // not on physical memory
    if (write_enabled == 0 && read_enabled == 0 && pte->ppage == PhysMem.size()) {
        // std::cout << "NOT ON PHYSICAL" << std::endl;

        /* move on to physical memory */
        // find a free page or evict to make a free page
        int free_pmem = get_ppage();

        // catch error in case write back fails
        if (free_pmem == -1) {
            return -1;
        }
        // std::cout << "Filename: " << bte->filename << std::endl;

        // find the according physical address and write back blocks
        void* phy_buff = static_cast<char*>(vm_physmem) + free_pmem * VM_PAGESIZE;

        // find the target filename and block
        const char* write_back_file = bte->filename.c_str();
        if (bte->filename == "SWAP") {
            write_back_file = nullptr;
        }
        unsigned int write_back_block = bte->block;
        // try to read into temp buff
        if (file_read(write_back_file, write_back_block, (void*) phy_buff) == -1) {
            // do nothing if read fails
            FreePhys.push(free_pmem);
            return -1;
        }

        MemQueue.push(free_pmem);

        // set up the physical memory block
        PhysMem[free_pmem].state = MEM_STATUS::REFERENCED;
        PhysMem[free_pmem].write_back_loc = std::make_unique<page_block_entry_t>(*bte);

        // const char* write_back_file = PhysMem[free_pmem].write_back_loc->filename.c_str();
        // if (PhysMem[free_pmem].write_back_loc->filename == "") {
        //     write_back_file = nullptr;
        // }
        // unsigned int write_back_block = PhysMem[free_pmem].write_back_loc->block;
        // // copy to the page
        // if (file_read(write_back_file, write_back_block, phy_buff) == -1) {
        //     FreePhys.push(free_pmem);
        //     return -1;
        // }

        // update page mapping and mark as read enabled
        std::set<page_table_entry_t*>& pte_set = get_mapped_pte_set(free_pmem);
        for (auto& pte : pte_set) {
            pte->ppage = free_pmem;
            pte->read_enable = 1;
            if (write_flag) {
                pte->write_enable = 1;
            }
        }
        // mark dirty bit
        if (write_flag) {
            PhysMem[free_pmem].dirty_bit = DB_STATUS::DIRTY;
        }
    }

    // page fault shouldn't occur if w/r==1
    assert(!(write_enabled == 1 && read_enabled == 1));

    printphysical();
    return 0;
}
/* VM_FAULT END */

/* VM_DESTROY START */
void vm_destroy() {
    // std::cout << "Before" << std::endl;
    printdebug();
    printphysical();
    // clean up the physical memory
    page_table_entry_t* page_table = ProcessPageTables[running_pid];
    page_block_entry_t* block_table = ProcessPageBlocks[running_pid];
    for (size_t vp = 0; vp < ProcessNextVPage[running_pid]; ++vp) {
        // std::cout << "VIRTUAL PAGE " << vp << "------" << std::endl;
        // find the mapped physical page (could be invalid)
        unsigned int mapped_pmem = page_table[vp].ppage;
        // find all other pte entries mapped to the same file/swap block
        std::set<page_table_entry_t*>& pte_set
          = MappedBlocks[std::string(block_table[vp].filename)][block_table[vp].block];

        // swap-back pages
        if (block_table[vp].filename == "SWAP") {
            // vacant swap file
            SwapBlocks[block_table[vp].block] = SWAP_STATUS::AVAILABLE;
            assert(FirstFreeSwap != block_table[vp].block);
            FirstFreeSwap = block_table[vp].block < FirstFreeSwap ? block_table[vp].block : FirstFreeSwap;
            // remove the entry from the Mapped Blocks
            assert_private_swap_block(pte_set);
            MappedBlocks[block_table[vp].filename].erase(block_table[vp].block);
            // clean up swap-back page on physical memory
            if (mapped_pmem != PhysMem.size() && PhysMem[mapped_pmem].state != MEM_STATUS::PINNED) {
                // clean up physical page
                reset_phys_page(mapped_pmem);
            }
        }
        // file back pages
        else {
            assert(pte_set.size() >= 1);
            // if it's the last mapping in the set
            if (pte_set.size() == 1) {
                // make ghost if on physical memory
                if (mapped_pmem != PhysMem.size()) {
                    // std::cout << "on physical" << std::endl;
                    page_table_entry_t* ghostpage = new page_table_entry_t;
                    ghostpage->ppage = (*pte_set.begin())->ppage;
                    ghostpage->read_enable = (*pte_set.begin())->read_enable;
                    ghostpage->write_enable = (*pte_set.begin())->write_enable;
                    // swap
                    pte_set.clear();
                    pte_set.insert(ghostpage);
                    assert(pte_set.size() == 1);
                    // mark as ghost
                    // page_block_entry_t* ghostblock = new page_block_entry_t;
                    // ghostblock->filename = block_table[vp].filename;
                    // ghostblock->block = block_table[vp].block;
                    // PhysMem[mapped_pmem].write_back_loc = ghostblock;
                    if (PhysMem[mapped_pmem].state == MEM_STATUS::REFERENCED) {
                        PhysMem[mapped_pmem].state = MEM_STATUS::GHOST_REF;
                    }
                    if (PhysMem[mapped_pmem].state == MEM_STATUS::PEND) {
                        PhysMem[mapped_pmem].state = MEM_STATUS::GHOST_PEND;
                    }
                    assert(PhysMem[mapped_pmem].state == MEM_STATUS::GHOST_REF
                           || PhysMem[mapped_pmem].state == MEM_STATUS::GHOST_PEND);
                }
                // otherwise remove the mapped block entry
                else {
                    // remove from the mapped blocks
                    pte_set.erase(page_table + vp);
                    assert(pte_set.size() == 0);
                    // remove the dict entry
                    MappedBlocks[std::string(block_table[vp].filename)].erase(block_table[vp].block);
                    if (MappedBlocks[std::string(block_table[vp].filename)].size() == 0) {
                        MappedBlocks.erase(std::string(block_table[vp].filename));
                    }
                }
            }
            // if not simply remove the mapping
            else {
                // remove from the mapped blocks
                pte_set.erase(page_table + vp);
                assert(pte_set.size() >= 1);
            }
        }

        // std::cout << "END VIRTUAL PAGE " << vp << "--" << std::endl;
    }

    // std::cout << "After" << std::endl;
    printdebug();
    printphysical();
    // move all free physical pages
    if (!MemQueue.empty()) {
        unsigned int curr_top = MemQueue.front();
        unsigned int initial_size = MemQueue.size();
        // std::cout << "Current TOP is " << curr_top << std::endl;
        // std::cout << "Mem Queue Size is " << MemQueue.size() << std::endl;
        for (size_t i = 0; i < initial_size; ++i) {
            // std::cout << i << std::endl;
            unsigned int tmp = MemQueue.front();
            MemQueue.pop();
            if (PhysMem[curr_top].state == MEM_STATUS::FREE) {
                curr_top = tmp;
                // std::cout << "Current TOP is " << curr_top << std::endl;
            }
            if (PhysMem[tmp].state == MEM_STATUS::FREE) {
                // std::cout << "Page " << tmp << " is free" << std::endl;
                FreePhys.push(tmp);
            } else {
                // std::cout << "Page " << tmp << " isn't free" << std::endl;
                MemQueue.push(tmp);
            }
        }
        // std::cout << "Mem Queue front is " << MemQueue.front() << std::endl;
        assert(MemQueue.empty() || curr_top == MemQueue.front());
    }
    // remove the pid's tables
    delete[] ProcessPageTables[running_pid];
    delete[] ProcessPageBlocks[running_pid];
    ProcessPageBlocks.erase(running_pid);
    ProcessPageTables.erase(running_pid);
    ProcessNextVPage.erase(running_pid);
}
/* VM_DESTROY END */

/* VM_MAP START */
std::string read_filename(const char* filename) {
    // std::cout << "Reading Filename" << std::endl;
    std::string filename_string;
    size_t i = 0;
    unsigned int vp;
    while (true) {
        // validate address
        if (!addressInArena(filename + i)) {
            return "FAIL";
        }
        // find virtual page
        vp = getArenaPageNumber(filename + i);
        // std::cout << "Virtual Page Found: " <<  vp << std::endl;
        if (!ProcessPageTables[running_pid][vp].read_enable) {
            if (vm_fault(filename + i, false) == -1) {
                // std::cout << "VM FAULT RETURNS -1" << std::endl;
                return "FAIL";
            }
        }
        // find physical address
        assert(ProcessPageTables[running_pid][vp].read_enable);
        unsigned int offset = reinterpret_cast<uintptr_t>(filename + i) % VM_PAGESIZE;
        unsigned int ppage = ProcessPageTables[running_pid][vp].ppage;
        char* phys_addr = static_cast<char*>(vm_physmem) + ppage * VM_PAGESIZE + offset;
        // read a byte
        filename_string.push_back(*phys_addr);
        // find end of string
        if (*phys_addr == '\0') {
            break;
        }
        i++;
    }
    // std::cout << filename_string << std::endl;
    return filename_string;
}

void* vm_map(const char* filename, unsigned int block) {
    // check if the arena is full
    if (ProcessNextVPage[running_pid] == VM_PAGENUM) {
        return nullptr;
    }
    // swap-back page
    if (filename == nullptr) {
        // check if there are enough swap blocks
        if (FirstFreeSwap == SwapBlocks.size()) {
            // eager reservation
            return nullptr;
        }
        // std::cout << "Next Free Swap Block: " << FirstFreeSwap << std::endl;
        // get the next available virtual address
        unsigned int vp = ProcessNextVPage[running_pid];
        ProcessNextVPage[running_pid]++;
        /* update the pte */
        // map to pinned page
        ProcessPageTables[running_pid][vp].ppage = 0;
        // set r = 1 and w = 0
        ProcessPageTables[running_pid][vp].read_enable = 1;
        ProcessPageTables[running_pid][vp].write_enable = 0;
        /* update the bte */
        // assign a swap block
        ProcessPageBlocks[running_pid][vp].filename = "SWAP";
        ProcessPageBlocks[running_pid][vp].block = FirstFreeSwap;
        // std::cout << "Virtual page " << vp << " mapped to swap block " << FirstFreeSwap << std::endl;
        // mark swap space as occupied
        SwapBlocks[FirstFreeSwap] = SWAP_STATUS::ALLOCATED;
        /* update the MappedBlocks */
        assert(MappedBlocks.find("SWAP") == MappedBlocks.end()
               || MappedBlocks["SWAP"].find(FirstFreeSwap) == MappedBlocks["SWAP"].end());
        MappedBlocks["SWAP"][FirstFreeSwap].insert(ProcessPageTables[running_pid] + vp);
        while (SwapBlocks[FirstFreeSwap] != SWAP_STATUS::AVAILABLE && FirstFreeSwap != SwapBlocks.size()) {
            FirstFreeSwap++;
        }
        printdebug();
        return getArenaPageAddress(vp);
    }
    // file-back page
    else {
        if (!addressInArena(filename)) {
            return nullptr;
        }
        std::string filestring = read_filename(filename);
        // std::cout << "INSIDE " << filestring << std::endl;
        if (filestring == "FAIL") {
            return nullptr;
        }
        // get the next available virtual address
        unsigned int vp = ProcessNextVPage[running_pid];
        ProcessNextVPage[running_pid]++;
        // record file block
        ProcessPageBlocks[running_pid][vp].filename = filestring;
        ProcessPageBlocks[running_pid][vp].block = block;

        // if the file block is already mapped
        if (MappedBlocks.find(filestring) != MappedBlocks.end()
            && MappedBlocks[filestring].find(block) != MappedBlocks[filestring].end()) {
            // std::cout << "File block is mapped" << std::endl;
            // make sure there's already one mapping existing
            assert(MappedBlocks[filestring][block].size() >= 1);
            // copy the mapping
            ProcessPageTables[running_pid][vp].ppage = (*MappedBlocks[filestring][block].begin())->ppage;
            ProcessPageTables[running_pid][vp].read_enable = (*MappedBlocks[filestring][block].begin())->read_enable;
            ProcessPageTables[running_pid][vp].write_enable = (*MappedBlocks[filestring][block].begin())->write_enable;
            // get the target physical mem page
            unsigned int target_ppage = ProcessPageTables[running_pid][vp].ppage;
            // std::cout << "Target PPAGE: " << target_ppage << std::endl;
            // check ghost page
            if (PhysMem[target_ppage].state == MEM_STATUS::GHOST_REF
                || PhysMem[target_ppage].state == MEM_STATUS::GHOST_PEND) {
                // std::cout << "File block is ghosted" << std::endl;
                assert(MappedBlocks[filestring][block].size() == 1);
                // mark as referenced/pending
                if (PhysMem[target_ppage].state == MEM_STATUS::GHOST_REF) {
                    PhysMem[target_ppage].state = MEM_STATUS::REFERENCED;
                    assert(ProcessPageTables[running_pid][vp].read_enable
                             + ProcessPageTables[running_pid][vp].write_enable
                           > 0);
                }
                if (PhysMem[target_ppage].state == MEM_STATUS::GHOST_PEND) {
                    PhysMem[target_ppage].state = MEM_STATUS::PEND;
                    ProcessPageTables[running_pid][vp].read_enable = 0;
                    ProcessPageTables[running_pid][vp].write_enable = 0;
                }
                // remove the ghost page
                delete *MappedBlocks[filestring][block].begin();
                MappedBlocks[filestring][block].clear();
                assert(MappedBlocks[filestring][block].size() == 0);
                // re-map the physical page
                // delete PhysMem[target_ppage].write_back_loc;
                // PhysMem[target_ppage].write_back_loc = &(ProcessPageBlocks[running_pid][vp]);
            }
        }
        // if the file block is not mapped yet
        else {
            // mark as on disk
            ProcessPageTables[running_pid][vp].ppage = PhysMem.size();
            ProcessPageTables[running_pid][vp].read_enable = 0;
            ProcessPageTables[running_pid][vp].write_enable = 0;
        }
        // insert the new entry
        MappedBlocks[filestring][block].insert(ProcessPageTables[running_pid] + vp);

        printdebug();
        return getArenaPageAddress(vp);
    }
}
/* VM_MAP END */
