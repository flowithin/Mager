#include "vm_pager.h"

#include "vm_arena.h"
#include "vm_pager_struct.h"

// Running Process Information
static uint32_t curr_pid;                         // id of the running process
static uint32_t bound;                            // the upper bound of valid virtual page number
static std::unordered_map<uint32_t, Pt> all_pt;   // page tables corresponding to process ids

// Swap Space Information
static uint32_t blcnt;                                                       // total swap block count
static uint32_t eblcnt;                                                      // empty swap block count
static std::queue<uint32_t> free_block;                                      // available swap blocks
static std::unordered_map<uint32_t, std::set<page_table_entry_t*>> swfile;   // swfile_block -> pte

// Physical Memory Information
static uint32_t pcnt;                             // physical page count
static uint32_t pinned;                           // pinned page number(0)
static std::queue<uint32_t> clock_q;              // all physical pages numbers except for the pinned page
static std::queue<uint32_t> free_ppage;           // free physical page numbers
static std::unordered_map<uint32_t, ppb> psuff;   // physical page bits
static std::unordered_map<uint32_t, std::pair<std::string, uint32_t>> ghost;   // pages created by deleted processes
static std::unordered_map<uint32_t, std::set<page_table_entry_t*>> core;       // all running/alive physical pages

// Virtual Page Information
static std::unordered_map<page_table_entry_t*, Infile> infile;   // write back location record for page table entries

// File-Back Page Info
static std::unordered_map<std::string, std::map<uint32_t, fbp>> filemap;   // all write-back files in use


/*
 * @brief the clock algorithm
 *
 * */
int runclock() {
    while (1) {
        // move front to tail
        clock_q.push(clock_q.front());
        clock_q.pop();
        // un-reference the referenced pages
        if (psuff[clock_q.back()].ref) {
            psuff[clock_q.back()].ref = 0;
            if (core.find(clock_q.back()) != core.end()) {
                // mark all mapped virtual pages as r=0 & w=0
                for (auto pte : core[clock_q.back()]) {
                    pte->read_enable = pte->write_enable = 0;
                }
            }
        }
        // return the page to evict and mark as referenced
        else {
            // the next one is at tail with ref 1
            psuff[clock_q.back()].ref = 1;
            return clock_q.back();
        }
    }
    return -1;
}

void vm_init(unsigned int memory_pages, unsigned int swap_blocks) {
    // initialize the swap space
    eblcnt = blcnt = swap_blocks;
    for (uint32_t i = 0; i < swap_blocks; i++) {
        free_block.push(i);
    }

    /* initialize the physical memory */
    pcnt = memory_pages;
    // pinned page init to zeros
    pinned = 0;   // may not be valid
    memset(static_cast<char*>(vm_physmem), 0, VM_PAGESIZE);
    // push other pages into the clock queue
    for (uint32_t i = 1; i < memory_pages; i++) {
        clock_q.push(i);
        free_ppage.push(i);
        psuff[i].ref = psuff[i].dirty = 0;
    }
}

int vm_create(pid_t parent_pid, pid_t child_pid) {
    uint32_t size = 0;    // parent page table size
    uint32_t numsw = 0;   // parent swap space size used

    // create a new page table for child process
    page_table_entry_t* child_pt = new page_table_entry_t[VM_ARENA_SIZE / VM_PAGESIZE];

    auto it = all_pt.find(parent_pid);
    if (it != all_pt.end()) {
        // parent_pid exists
        page_table_entry_t* parent_pt = it->second.st.get();
        // NOTE: The following code is for 6 credit version
        if (it->second.numsw > free_block.size()) {
            return -1;
        }
        size = it->second.size;
        for (size_t i = 0; i < size; i++) {
            // copy on write
            parent_pt[i].write_enable = 0;
            // duplicate
            child_pt[i] = parent_pt[i];
            // update core
            core[child_pt[i].ppage].insert(child_pt + i);
            // update infile
            auto _it = infile.find(parent_pt + i);
            assert(_it != infile.end());
            if (_it->second.ftype == file_t::FILE_B) {
                infile[child_pt + i]
                  = { _it->second.ftype, _it->second.infile, _it->second.block, _it->second.filename };
                // update filemap
                filemap[_it->second.filename][_it->second.block].vpset.insert(child_pt + i);
            } else {
                infile[child_pt + i]
                  = { _it->second.ftype, _it->second.infile, free_block.front(), _it->second.filename };
                swfile[free_block.front()].insert(child_pt + i);
                free_block.pop();
                eblcnt--;
            }
        }
        numsw = it->second.numsw;
    }   // if it not end
    // empty the arena (just a try)
    for (size_t i = size; i < VM_ARENA_SIZE / VM_PAGESIZE; i++) {
        child_pt[i].ppage = child_pt[i].write_enable = child_pt[i].read_enable = 0;
    }
    eblcnt -= numsw;
    // record the new page table for child process
    all_pt[child_pid] = Pt({ size, numsw, std::unique_ptr<page_table_entry_t[]>(child_pt) });
    return 0;
}

void vm_switch(pid_t pid) {
    // update running process info
    curr_pid = pid;
    bound = all_pt[pid].size;
    page_table_base_register = all_pt[pid].st.get();
}


/*
 *  @brief evict one page from the current clock queue
 *
 * */
int pm_evict() {
    // get a physical page to move things in
    int ppage = runclock();

    // if the page is a left over from a previously destroyed process
    if (ghost.find(ppage) != ghost.end()) {
        // clean up the page
        if (psuff[ppage].dirty) {
            // write back dirty page
            if (file_write(ghost[ppage].first.c_str(), ghost[ppage].second, (char*) vm_physmem + ppage * VM_PAGESIZE)
                == -1) {
                free_ppage.push(ppage);
                psuff[ppage].ref = psuff[ppage].dirty = 0;
                return -1;
            }
            // mark as clean
            psuff[ppage].dirty = 0;
        }
        // remove its record
        filemap[ghost[ppage].first].erase(ghost[ppage].second);
        // if no longer referenced by other process
        if (filemap[ghost[ppage].first].empty()) {
            filemap.erase(ghost[ppage].first);
        }
        ghost.erase(ppage);
        return ppage;
    }

    // if the page is free or part of a running process
    assert(core.find(ppage) != core.end());
    // randomly choose one to see if they are in file
    page_table_entry_t* pte = *core[ppage].begin();
    auto _it = infile.find(pte);
    assert(_it != infile.end());
    assert(free_block.size() <= blcnt);

    // retrieve the write back filename and block index
    const char* filename;
    if (_it->second.ftype == file_t::SWAP) {
        filename = nullptr;
    } else {
        filename = _it->second.filename.c_str();
    }
    uint32_t block = infile[pte].block;

    // write back if dirty
    if (psuff[ppage].dirty) {
        // write back if dirty
        void* eaddr = (char*) (vm_physmem) + ppage * VM_PAGESIZE;
        if (file_write(filename, block, eaddr) == -1) {
            free_ppage.push(ppage);
            psuff[ppage].ref = psuff[ppage].dirty = 0;
            return -1;
        }
        // clear the dirty bit
        psuff[ppage].dirty = 0;
    }

    // downward all pte
    if (_it->second.ftype == file_t::FILE_B) {
        // not in physmem anymore
        for (auto v : filemap[filename][_it->second.block].vpset) {
            v->write_enable = v->read_enable = 0;
            infile[v].infile = true;
        }
    } else {
        for (auto v : swfile[block]) {
            v->write_enable = v->read_enable = 0;
            infile[v].infile = true;
        }
    }
    // update core
    core.erase(ppage);
    return ppage;
}

/*
 *  @brief allocate one page and clear its traces
 *
 *
 * */
int alloc() {
    int ppage;
    // if a free page is available
    if (!free_ppage.empty()) {
        ppage = free_ppage.front();
        free_ppage.pop();
        // want the clock to be aware of the new ppage
        for (size_t i = 0; i < pcnt - 1; i++) {
            if (clock_q.front() != (size_t) ppage) {
                clock_q.push(clock_q.front());
            }
            clock_q.pop();
        }
        clock_q.push(ppage);
        psuff[ppage].ref = 1;
    }
    // otherwise evict to make space
    else {
        ppage = pm_evict();
        if (ppage == -1) return -1;
    }
    return ppage;
}

/*
 *  @brief copy on write operation
 *  @param pte: ptr to page table entry interested in
 *  @param content: the content in its old page
 * */
int cow(page_table_entry_t* pte, char* content) {
    // allocate one ppage
    assert(filemap.find(pte) != filemap.end());
    assert(ghost.find(pte->ppage) != ghost.end());
    int ppage = alloc();
    if (ppage == -1) {
        return -1;
    }
    // copy the page to a new physical page
    std::memcpy(static_cast<char*>(vm_physmem) + ppage * VM_PAGESIZE, content, VM_PAGESIZE);

    // update core
    core[pte->ppage].erase(pte);
    if (core[pte->ppage].empty()) {
        core.erase(pte->ppage);
    }
    pte->ppage = ppage;
    core[pte->ppage].insert(pte);

    // update ref and dirty and write read
    psuff[pte->ppage].dirty = 1;
    pte->read_enable = pte->write_enable = 1;
    return 0;
}

/*
 *  @brief check if a page table entry needs copy on write
 *  @param pte: ptr to page table entry interested in
 *
 * */
bool is_cow(page_table_entry_t* pte) {
    // pte should exist in core and also in mem
    return (core.find(pte->ppage) != core.end() && core[pte->ppage].size() > 1 && infile[pte].ftype == file_t::SWAP
            && !infile[pte].infile)
        || pte->ppage == pinned;
}

int vm_fault(const void* addr, bool write_flag) {
    // find the virtual page corresponding to the address
    uint64_t page = (reinterpret_cast<uint64_t>(addr) - reinterpret_cast<uint64_t>(VM_ARENA_BASEADDR)) >> 16;

    // check if virtual page is valid
    if (page >= bound || reinterpret_cast<uint64_t>(addr) < reinterpret_cast<uint64_t>(VM_ARENA_BASEADDR)) {
        return -1;
    }

    // get the corresponding page table entry
    page_table_entry_t* pte = page_table_base_register + page;
    auto it = infile.find(pte);
    assert(it != infile.end());
    // target page on disk
    if (it->second.infile) {
        // on disk
        // find a page
        int epage = alloc();
        if (epage == -1) return -1;

        // physical page address
        void* eaddr = static_cast<char*>(vm_physmem) + epage * VM_PAGESIZE;

        if (file_read(it->second.ftype == file_t::FILE_B ? it->second.filename.c_str() : nullptr, it->second.block,
                      eaddr)
            == -1) {
            free_ppage.push(epage);
            psuff[epage].ref = psuff[epage].dirty = 0;
            return -1;
        }
        // mark all associated entries as lifted
        auto& lifted = (it->second.ftype == file_t::SWAP) ? swfile[it->second.block]
                                                          : filemap[it->second.filename][it->second.block].vpset;
        for (auto l : lifted) {
            infile[l].infile = false;
            l->ppage = epage;
            l->read_enable = 1;
            // update all ptes associated with the physical page
            core[epage].insert(l);
        }
        // update the filemap for file-back pages
        if (it->second.ftype == file_t::FILE_B) {
            filemap[it->second.filename][it->second.block].ppage = epage;
        }
    }

    // mark as read enable
    pte->read_enable = 1;
    // if the page is not mapped to the pinned page
    auto it_psuff = psuff.find(pte->ppage);
    if (it_psuff != psuff.end()) {
        it_psuff->second.ref = 1;
        if (it_psuff->second.dirty) pte->write_enable = 1;   // if dirty no need to set dirty
        if (it->second.ftype == file_t::FILE_B) {
            auto& lifted = filemap[it->second.filename][it->second.block].vpset;
            for (auto l : lifted) {
                *l = *pte;
            }
        }
    }   // except for pinned page

    // grant write permission
    bool _is_cow = is_cow(pte);
    if (write_flag) {
        if (_is_cow) {
            // copy on write
            if (cow(pte, static_cast<char*>(vm_physmem) + pte->ppage * VM_PAGESIZE) == -1) {
                return -1;
            }
        } else {
            assert(it_psuff != psuff.end());   // shouldn't be pinned page
            it_psuff->second.dirty = 1;
            // pte->ppage should be updated by now
            for (auto p : core[pte->ppage]) {
                p->read_enable = p->write_enable = 1;
            }
        }
    }

    return 0;
}

/*
 *  @brief from usr's virtual address to the virtual page number it is in
 *
 *  @param addr: the virtual address
 *
 * */
uint32_t a2p(const char* addr) {
    return (reinterpret_cast<uintptr_t>(addr) - reinterpret_cast<uintptr_t>(VM_ARENA_BASEADDR)) >> 16;
}

/*
 *  @brief from usr's virtual address to physical memory content
 *
 *  @param addr: usr's virtual address
 *
 * */
char mem(const char* addr) {
    return static_cast<char*>(vm_physmem)[page_table_base_register[a2p(addr)].ppage * VM_PAGESIZE
                                          + (reinterpret_cast<uintptr_t>(addr) & 0xFFFF)];
}

/*
 *  @brief from virtual address to the c string it points to
 *
 *  @param filename: usr's virtual address
 *
 * */
std::string vm_to_string(const char* filename) {
    // validate virtual address
    if (filename < static_cast<char*>(VM_ARENA_BASEADDR)
        || filename >= (char*) (static_cast<char*>(VM_ARENA_BASEADDR) + VM_ARENA_SIZE)) {
        return "@FAULT";
    }
    // get the initial virtual page number
    uint32_t vpage = a2p(filename);
    uint32_t i = 0;
    // result string holder
    std::string rs;
    while (1) {
        auto vaddr = static_cast<char*>(VM_ARENA_BASEADDR) + vpage * VM_PAGESIZE;
        if (page_table_base_register[vpage].read_enable == 0 && vm_fault(vaddr, 0) == -1) {
            return "@FAULT";
        }
        // break if reach end
        if (mem(filename + i) == '\0') {
            break;
        }
        // get the string char by char
        rs += mem(filename + i++);
        vpage = a2p(filename + i);
    }

    return rs;
}

void* vm_map(const char* filename, unsigned int block) {
    // check full arena or full swap space
    if ((filename == nullptr && free_block.empty()) || bound == VM_ARENA_SIZE / VM_PAGESIZE) {
        return nullptr;
    }

    // get the next page table entry
    page_table_entry_t* new_entry = page_table_base_register + all_pt[curr_pid].size;

    // file-backed
    if (filename != nullptr) {
        // get the filename from virtual space
        std::string file_str = vm_to_string(filename);
        if (file_str == "@FAULT") {
            return nullptr;
        }
        auto it = filemap.find(file_str);
        // if a file has already been mapped, update to match the rest
        if (it != filemap.end() && it->second.find(block) != it->second.end()) {
            // file matched && block matched
            auto _it = it->second.find(block);
            new_entry->ppage = _it->second.ppage;
            // if ghost page
            if (ghost.find(_it->second.ppage) != ghost.end()) {
                ghost.erase(_it->second.ppage);
                new_entry->read_enable = psuff[_it->second.ppage].ref;
                new_entry->write_enable = psuff[_it->second.ppage].ref && psuff[_it->second.ppage].dirty;
            }
            // if not a ghost page
            else {
                *new_entry = **_it->second.vpset.begin();
            }
            // if is a ghost page -> not infile
            bool is_infile = (!_it->second.vpset.empty() && infile[*_it->second.vpset.begin()].infile);
            // update infile
            infile[new_entry] = Infile { file_t::FILE_B, is_infile, block, file_str };
            // update core only if in mem
            if (!is_infile) {
                core[new_entry->ppage].insert(new_entry);
            }
            // filemap update
            _it->second.vpset.insert(new_entry);
        }
        // new file block used
        else {
            // update filemap
            filemap[file_str][block].vpset.insert(new_entry);
            *new_entry = { .ppage = pinned, .read_enable = 0, .write_enable = 0 };
            infile[new_entry] = Infile { file_t::FILE_B, true, block, file_str };
        }
    }
    // swap-back page
    else {
        // map a new page to the pinned page
        infile[new_entry] = { file_t::SWAP, false, free_block.front(), "@SWAP" };
        swfile[free_block.front()].insert(new_entry);
        free_block.pop();
        all_pt[curr_pid].numsw++;
        eblcnt--;
        *new_entry = { .ppage = pinned, .read_enable = 1, .write_enable = 0 };
        core[pinned].insert(new_entry);
    }

    // update core
    bound = ++all_pt[curr_pid].size;
    return (char*) VM_ARENA_BASEADDR + (bound - 1) * VM_PAGESIZE;
}

/*
 *  @brief clean up for the page table entry
 *
 *  @param pte: ptr to page table entry
 *
 * */
void vm_discard(page_table_entry_t* pte) {
    // find the target pte's file block info
    auto it = infile.find(pte);
    assert(it != infile.end());

    // free the blocks used and write back
    switch (it->second.ftype) {
        // swap back
    case file_t::SWAP: {
        // the last one leaves free up the block
        if (swfile[it->second.block].size() == 1) {
            free_block.push(it->second.block);
            swfile.erase(it->second.block);
            eblcnt++;
        } else {
            swfile[it->second.block].erase(pte);
        }
        break;
    }
        // file back
    case file_t::FILE_B: {
        // deeply clean filemap
        assert(filemap[it->second.filename][it->second.block].vpset.find(pte)
               != filemap[it->second.filename][it->second.block].vpset.end());
        // deeply remove pte
        filemap[it->second.filename][it->second.block].vpset.erase(pte);
        // if on disk
        if (it->second.infile) {
            // remove all trace!
            if (filemap[it->second.filename][it->second.block].vpset.empty()) {
                filemap[it->second.filename].erase(it->second.block);
            }
            if (filemap[it->second.filename].empty()) {
                filemap.erase(it->second.filename);
            }
        }
        break;
    }
    }
    // in physical mem
    if (!it->second.infile) {
        // private file-back page
        if (it->second.ftype == file_t::FILE_B && core[pte->ppage].size() == 1) {
            ghost[pte->ppage] = std::pair(it->second.filename, it->second.block);
        }
        // swap-back pages not mapped to pinned
        else if (core[pte->ppage].size() == 1 && pte->ppage != pinned) {
            // give back the physical page
            free_ppage.push(pte->ppage);
            psuff[pte->ppage].ref = psuff[pte->ppage].dirty = 0;
        }
        // shared file-back pages
        if (core[pte->ppage].size() > 1) {
            core[pte->ppage].erase(pte);
        }
        // all pages except for shared file-back pages
        else {
            core.erase(pte->ppage);
        }
    }
    // update infile
    infile.erase(pte);
}


void vm_destroy() {
    // free all physical mem
    for (size_t i = 0; i < all_pt[curr_pid].size; i++) {
        vm_discard(page_table_base_register + i);
    }
    // free page table
    all_pt[curr_pid].st.reset();
    all_pt.erase(curr_pid);
}
