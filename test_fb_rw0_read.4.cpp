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
  /*std::cout << "pid = " << pid << '\n';*/
  /* Allocate swap-backed page from the arena */
  char* filename = static_cast<char *>(vm_map(nullptr, 0));
  char* filename1 = static_cast<char *>(vm_map(nullptr, 0));
  char* filename2 = static_cast<char *>(vm_map(nullptr, 0));
 /* Write the name of the file that will be mapped */
  //all pointing to 0
  strcpy(filename, "papers.txt");
  char* p;

  //these should evict all the pages
  strcpy(filename1, "papers.txt");
  strcpy(filename2, "papers.txt");
  /*strcpy(filename1, "papers.txt");*/
  //all ppage has been written to (swap)
  /* Map a page from the specified file */
  p = static_cast<char *>(vm_map (filename, 0));
  char* p1 = static_cast<char *>(vm_map (filename1, 0));
  char* p2 = static_cast<char *>(vm_map (filename1, 0));
  char* p3 = static_cast<char *>(vm_map (filename1, 1));
  char* p4 = static_cast<char *>(vm_map (filename1, 2));
  char x = p[0];
  char y = p3[0];
  //now should have rw=0
  char z = p4[0]++;
  char f= filename1[0];
  //shoud now have rw=0;
  z = p4[0];

}
