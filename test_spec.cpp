#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;

int main() {
    /* Allocate swap-backed page from the arena */
    char* filename = static_cast<char *>(vm_map(nullptr, 0));
     filename = static_cast<char *>(vm_map(nullptr, 0));
     filename = static_cast<char *>(vm_map(nullptr, 0));
     filename = static_cast<char *>(vm_map(nullptr, 0));
     filename = static_cast<char *>(vm_map(nullptr, 0));
     filename = static_cast<char *>(vm_map(nullptr, 0));
    /* Write the name of the file that will be mapped */
    strcpy(filename, "papers.txt");

    /* Map a page from the specified file */
    char* p = static_cast<char *>(vm_map (filename, 0));

    /* Print the first part of the paper */
    for (unsigned int i=0; i<1930; i++) {
	cout << p[i];
    }
}
