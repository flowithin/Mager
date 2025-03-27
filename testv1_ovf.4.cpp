#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"
#include "vm_arena.h"

using std::cout;

int main() {
  /* Allocate swap-backed page from the arena */
  char* filename = static_cast<char *>(vm_map(nullptr, 0));
  strcpy(filename, "papers.txt");
  /*char* p = static_cast<char *>(vm_map(filename - 1, 0));*/
  strcpy((char*)0, "overflow man!");
 
}
