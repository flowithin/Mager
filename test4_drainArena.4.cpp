#include <cstring>
#include <iostream>

#include <unistd.h>

#include "vm_app.h"

using std::cout;

int main() {
    if (fork() != 0) {   // parent
        std::cout << "Starting Parent" << std::endl;
        for (size_t i = 0; i < 255; ++i) {
            /* Allocate swap-backed page from the arena */
            char* filename = static_cast<char*>(vm_map(nullptr, 0));
            if (filename == nullptr) {
                std::cout << "Parent Yielding" << std::endl;
                vm_yield();
                break;
            }
        }
        std::cout << "Parent exits" << std::endl;
        return 0;
    } else {   // child
        std::cout << "Starting Child" << std::endl;
        char* swapblock = static_cast<char*>(vm_map(nullptr, 0));
        std::strcpy(swapblock, "papers.txt");
        std::cout << "Here " << swapblock << std::endl;
        for (size_t i = 0; i < 300; ++i) {
            /* Allocate swap-backed page from the arena */
            auto* page = static_cast<char*>(vm_map(swapblock, 0));
            if (page == nullptr) {
                vm_yield();
                break;
            } else {
                std::cout << "Mapping File-Back Page " << i << " to VP "
                          << (reinterpret_cast<const char*>(page) - static_cast<char*>(VM_ARENA_BASEADDR)) / VM_PAGESIZE
                          << std::endl;
                std::cout << page[i] << std::endl;
                char tmp = page[i];
                page[i] = ' ';
                page[i] = tmp;
            }
        }
        std::cout << "Child exits" << std::endl;
    }
}