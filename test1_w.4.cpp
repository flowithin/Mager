#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"
#include "vm_arena.h"

using std::cout;

int main() {
  /* Allocate swap-backed page from the arena */
  char* filename = static_cast<char *>(vm_map(nullptr, 0));
  char* filename1 = static_cast<char *>(vm_map(nullptr, 0));
  char* filename2 = static_cast<char *>(vm_map(nullptr, 0));
  char* filename3 = static_cast<char *>(vm_map(nullptr, 0));
  char* filename4 = static_cast<char *>(vm_map(nullptr, 0));

  /* Write the name of the file that will be mapped */
  //all pointing to 0
  strcpy(filename, "papers.txt");
  strcpy(filename1, "papers.txt");
  strcpy(filename2, "papers.txt");
  char* p = static_cast<char *>(vm_map (filename, 0));
}
