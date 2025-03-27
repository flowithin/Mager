#include <cassert>
#include <cstring>
#include <iostream>

#include <unistd.h>

#include "vm_app.h"

int main() {
    /* 4 pages of physical memory in the system */
    if (fork() != 0) {   // parent
        auto* page0 = static_cast<char*>(vm_map(nullptr, 0));
        auto* page1 = static_cast<char*>(vm_map(nullptr, 0));
        auto* page2 = static_cast<char*>(vm_map(nullptr, 0));
        page2[0] = 'a';
        page1[0] = 'a';
        page0[0] = 'a';
        page0[0] = 'a';
        page1[0] = 'a';
        page2[0] = 'a';
        vm_yield();
    } else {   // child
        std::cout << "Child Starts" << std::endl;
        auto* page0 = static_cast<char*>(vm_map(nullptr, 0));
        std::cout << "Child Starts" << std::endl;
        std::strcpy(page0, "Hello, world!");
        page0 = static_cast<char*>(vm_map(nullptr, 0));
        std::cout << "Child Starts" << std::endl;
        std::strcpy(page0, "Hello, world!");
        page0 = static_cast<char*>(vm_map(nullptr, 0));
        std::cout << "Child Starts" << std::endl;
        std::strcpy(page0, "Hello, world!");
    }
}
