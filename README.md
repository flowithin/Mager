# Virtual Memory Pager

This project implements a virtual memory paging system with support for both **swap-backed** and **file-backed** pages. It uses a **clock (second-chance)** page replacement algorithm, supports **copy-on-write**, and maintains separate page tables for each process. It is designed for a simulated operating system or OS-level coursework environment.

---

## 📐 Spec Overview

### 🔧 Key Concepts

- **Pinned Memory**: Newly created swap-backed pages initially point to a single read-only zero page until modified.
- **Eager Swap Reservation**:  
  - `vm_create` fails and returns `-1` if insufficient swap space exists.
- **Page Fault Handling**:  
  - Triggered when accessing a virtual page with invalid permissions (e.g., R == W == 0).
- **Clock Queue (Second Chance)**:  
  - Per physical page, maintains:
    - `referenced bit`
    - `dirty bit`
- **Arena**:  
  - Each process has an "arena" representing its private virtual address space managed by the pager.
- **Swap-backed vs. File-backed Differences**:  
  - File-backed pages may be reloaded from disk, while swap-backed pages must be preserved during eviction.

---

## 🧱 Data Structures

```cpp
struct PMB {
  bool ref_b;     // Referenced bit
  bool dirty_b;   // Dirty bit
};
```

- `map_table`: `hashmap<uint32_t, PMB>` — physical memory → reference/dirty info  
- `page_table`: `page_table_entry_t*` — current page table  
- `all_pt`: `hashmap<pid, unique_ptr<pair<uint, page_table_entry_t*>>>`  
- `infile_vm`: `hashmap<page_table_entry_t*, pair<char*, uint>>`  
- `file_vm_map`: `hashmap<string, vector<pair<uint, page_table_entry_t*>>>`  
- `core_map`: `hashmap<uint, vector<page_table_entry_t*>>` — physical page → VM entries  
- `clock_queue`: `queue<uint32_t>` — stores physical page numbers for eviction

### 🔒 Global State

- `bound`: arena upper limit  
- `pcnt`: total physical page count  
- `blcnt`: total swap block count  
- `pined`: pinned page count  
- `curr_pid`: currently active process

---

## 🧠 Function Descriptions

### `vm_init(memory_pages, swap_blocks)`

- Initializes the memory system
- Pins the zero page
- Initializes the clock queue and page metadata

### `vm_create(parent_pid, child_pid)`

- Copies parent’s arena into child
- Allocates new page table
- Duplicates relevant page table entries
- Handles copy-on-write setup

### `vm_switch(pid)`

- Switches active process context
- Updates page table base register
- Updates `curr_pid` and `bound`

### `vm_map(filename, block)`

- Maps a new page:
  - File-backed: associates with file and block
  - Swap-backed: reserves swap space
- Adds a new page to the current process’s page table
- Sets up file-backed pinning if necessary

### `vm_fault(addr, write_flag)`

- Handles page faults
  - Invalid address → error
  - Pinned page write → allocate real page and copy
  - Copy-on-write → duplicate page
  - Infile page → reload from file
- Eviction via clock queue if necessary
- Updates reference/dirty bits

### `vm_destroy()`

- Frees all physical memory and swap blocks associated with the process
- Cleans up data structures

---

## 🧪 Test Coverage

### ✅ Basic Functionality

- [x] Page eviction
- [x] Free page allocation
- [x] Process destruction
- [x] Forking and context switching
- [x] Swap and file write behavior
- [x] Reserve-check for swap-backed pages
- [x] Read pinned pages
- [x] Copy-on-write

### 📁 File-backed Pages

- [x] Fault on file not in memory
- [x] `vm_to_string` fault test
- [x] Different block, same file
- [x] File not found
- [x] Mapping during multiple phases
- [x] Forking in different phases

---

## 📅 Notes and History

### 🧟 Ghost Pages (Implemented Mar 18)

- Support for delayed actual memory allocation until access

### 🧠 COW (Copy-on-Write)

- Read-only sharing until write
- Handled in `vm_fault`
- Update page table permissions and metadata

### 🛠 Fixes and Improvements

- [x] File-backed page reuse improved
- [x] Ghost page writes flushed correctly
- [x] Dirty/ref bit management fixed
- [x] Filename read permission issues resolved
- [x] Memory leak and page table copy overhead addressed
- [x] Integer overflow guards added

---

## 🤔 Implementation Insights

- Only allocate physical memory in `alloc()`
- Only free memory in `destroy()`
- Use hashmaps for flexible and dynamic memory tracking
- Ensure IO operations are interruptible for correctness under context switching
- Track corner cases like:
  - Mapping while evicting
  - Forking during I/O
  - Copy-on-write across forked processes

---

## 🔍 Debugging & Testing Tips

- Check reference and dirty bit correctness on each access
- Log page table changes and core map status
- Use assertion-heavy test cases for edge scenarios
- Simulate frequent context switches to surface IO bugs
- Track memory usage to prevent leaks or overflow
