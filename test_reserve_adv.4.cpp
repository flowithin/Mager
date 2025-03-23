#include <cassert>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;

int main() {
  char* filename;
  /* Allocate swap-backed page from the arena */
  for(int i = 0; i < 256; i++){
    filename = static_cast<char *>(vm_map(nullptr, 0));
}
  pid_t pid = fork();
  vm_yield();
  /* Write the name of the file that will be mapped */
  //all pointing to 0
  strcpy(filename, "papers.txt");
  /*strcpy(filename1, "papers.txt");*/
  //all ppage has been written to (swap)

  /* Map a page from the specified file */
  char* p = static_cast<char *>(vm_map (filename, 0));
  std::cout << std::hex << p << '\n';
  assert(p != nullptr);
  std::cout << "hey" << p[0] << '\n';
  /* Print the first part of the paper */
  for (unsigned int i=0; i<10; i++) {
    //need evicting(bring from file)
    cout << p[i];
    //write fault
    p[i] ++;
  }
  for (unsigned int i=0; i<10; i++) {
    cout << p[i];
  }
}
