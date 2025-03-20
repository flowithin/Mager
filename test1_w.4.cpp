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
  char* p = static_cast<char *>(vm_map (filename, 0));
  char x;
  std::cout << p[0];
  char* p1 = static_cast<char *>(vm_map (filename, 1));
  x = p1[0]++;
  strcpy(filename1, "papers.txt");
  strcpy(filename2, "papers.txt");
  strcpy(filename3, "papers.txt");
  x = p1[0]++;
  p[0]++;
  strcpy(filename1, "papers.txt");
  strcpy(filename2, "papers.txt");
  strcpy(filename3, "papers.txt");
  x = p1[0]++;
  x = p1[0]++;
  p[0]++;
  std::cout << p[0] << p1[0];
  std::cout << x;
  /*strcpy(filename1, "papers.txt");*/
  //all ppage has been written to (swap)

  /* Map a page from the specified file */

  /* Print the first part of the paper */

}
