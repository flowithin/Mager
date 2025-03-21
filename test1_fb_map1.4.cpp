#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;

/*
 * expected behavior:
 * the parent left ghost pages 1 2 3
 * the children upon mapping, will take those and grab to use 
 * inside the if statement
 * */
int main() {
  pid_t pid = fork();
  /*std::cout << "pid = " << pid << '\n';*/
  /* Allocate swap-backed page from the arena */
  char* filename = static_cast<char *>(vm_map(nullptr, 0));
  char* filename1 = static_cast<char *>(vm_map(nullptr, 0));
  char* filename2 = static_cast<char *>(vm_map(nullptr, 0));
  char* filename3 = static_cast<char *>(vm_map(nullptr, 0));
 /* Write the name of the file that will be mapped */

  strcpy(filename, "papers.txt");
  //evicting
  strcpy(filename1, "papers.txt");
  char* p = static_cast<char *>(vm_map (filename, 0));
  strcpy(filename2, "papers.txt");
  std::cout <<p[0];
  //p becomes ghost page

}
