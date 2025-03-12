#include <iostream>
#include <cstring>
#include <sched.h>
#include <unistd.h>
#include "vm_app.h"

using std::cout;

int main() {
  /* Allocate swap-backed page from the arena */
  char* filename = static_cast<char *>(vm_map(nullptr, 0));
  char* filename1 = static_cast<char *>(vm_map(nullptr, 0));
  filename = static_cast<char *>(vm_map(nullptr, 0));
  filename = static_cast<char *>(vm_map(nullptr, 0));
  filename = static_cast<char *>(vm_map(nullptr, 0));
  filename = static_cast<char *>(vm_map(nullptr, 0));
  filename = static_cast<char *>(vm_map(nullptr, 0));
  /* Write the name of the file that will be mapped */
  strcpy(filename, "papers.txt");
  strcpy(filename1, "papers.txt");
  /*strcpy(filename1, "papers.txt");*/
  pid_t pid;
  pid_t pid2;
  pid = fork();

  /* Map a page from the specified file */
  char* p = static_cast<char *>(vm_map (filename, 0));
  char* p1 = static_cast<char *>(vm_map (filename1, 0));
  pid2 = fork();

  /* Print the first part of the paper */
  for (unsigned int i=0; i<10; i++) {
    cout << p[i];
    p[i] ++;
  }

  cout << '\n';
  for (unsigned int i=0; i<10; i++) {
    cout << p1[i];
    p1[i] ++;
  }
 
  cout << '\n';
  for (unsigned int i=0; i<10; i++) {
    cout << p[i];
  }
  vm_yield();
}
