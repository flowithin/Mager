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
  strcpy(filename+VM_PAGESIZE-4, "papers.txt");
  strcpy(filename2 + 10, "papers.txt");
  strcpy(filename3 , "papers.txt");
  /*strcpy(filename1, "papers.txt");*/
  //all ppage has been written to (swap)

  /* Map a page from the specified file */
  char* p = static_cast<char *>(vm_map (filename+VM_PAGESIZE-4, 0));

  /* Print the first part of the paper */
  for (unsigned int i=0; i<10; i++) {
    //need evicting(bring from file)
    cout << p[i];
    //write fault
    p[i] ++;
  }

}
