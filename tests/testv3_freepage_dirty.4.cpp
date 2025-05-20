#include <iostream>
#include <cstring>
#include <sched.h>
#include <unistd.h>
#include "vm_app.h"

using std::cout;

int main() {
  /* Allocate swap-backed page from the arena */

  pid_t pid = fork();
  char* filename = static_cast<char *>(vm_map(nullptr, 0));
  char* filename1 = static_cast<char *>(vm_map(nullptr, 0));
  char* filename2 = static_cast<char *>(vm_map(nullptr, 0));
  char* filename3 = static_cast<char *>(vm_map(nullptr, 0));
  char* filename4 = static_cast<char *>(vm_map(nullptr, 0));
  strcpy(filename1, "papers.txt");
  strcpy(filename, "pa");
  strcpy(filename2, "papers.txt");
  strcpy(filename3, "papers.txt");
  strcpy(filename4, "papers.txt");
  /*strcpy(filename1, "papers.txt");*/
  //all ppage has been written to (swap)

  /* Map a page from the specified file */
  char* p = static_cast<char *>(vm_map (filename, 0));
  std::cout << p[0];
}
