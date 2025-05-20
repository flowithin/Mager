#include <cassert>
#include <cstring>
#include <iostream>

#include <unistd.h>

#include "vm_app.h"

using std::cout;

int main() {        /* 5 pages of physical memory in the system */
    if (fork()) {   // parent
        auto* filename = static_cast<char*>(vm_map(nullptr, 0));
        std::strcpy(filename, "data4.bin");
        auto* fb_page1 = static_cast<char*>(vm_map(filename, 0));
        auto* fb_page2 = static_cast<char*>(vm_map(filename, 1));
        auto* fb_page3 = static_cast<char*>(vm_map(filename, 2));
        // 1 pinned page, 1 swap page, 3 file pages
        // physical memory full & read-only
        char tmp = fb_page1[0];
        fb_page1[0] = fb_page2[0] = fb_page3[0] = tmp;
        // physical memory full & read-write
        vm_yield();
        // end process
    } else {   // child
        vm_yield();
        // 2,3,4 are 3 ghost pages of parents, 1 free page
        auto* filename = static_cast<char*>(vm_map(nullptr, 0));
        std::strcpy(filename, "data4.bin");
        auto* fb_page1 = static_cast<char*>(vm_map(filename, 3));
        auto* fb_page2 = static_cast<char*>(vm_map(filename, 4));
        auto* fb_page3 = static_cast<char*>(vm_map(filename, 5));
        char tmp = fb_page1[0];
        tmp = fb_page2[0];
        tmp = fb_page3[0];   // page fault
        fb_page3[0] = tmp;
    }
}