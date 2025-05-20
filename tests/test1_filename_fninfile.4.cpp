#include <iostream>
#include <cstring>
#include <sched.h>
#include <unistd.h>
#include "vm_app.h"
#include "vm_arena.h"

using std::cout;

int main() {
  /* Allocate swap-backed page from the arena */
  char* filename = static_cast<char *>(vm_map(nullptr, 0));
  /* Write the name of the file that will be mapped */
  //all pointing to 0
  strcpy(filename, "test.txt");
  /*strcpy(filename1, "papers.txt");*/
  //all ppage has been written to (swap)

  /* Map a page from the specified file */

  char* p = static_cast<char *>(vm_map (filename, 0));
  char* p1 = static_cast<char *>(vm_map (p, 0));

  /* Print the first part of the paper */
  for (unsigned int i=0; i<10; i++) {
    //need evicting(bring from file)
    cout << p[i];
    //write fault
    p[i] ++;
  }

}
