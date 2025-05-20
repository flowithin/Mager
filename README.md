
from pathlib import Path

readme_content = """
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
