#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;

int main() {
  /* Allocate swap-backed page from the arena */
  char* filename = static_cast<char *>(vm_map(nullptr, 0));
  char* filename1 = static_cast<char *>(vm_map(nullptr, 0));
  char* filename2 = static_cast<char *>(vm_map(nullptr, 0));
  /* Write the name of the file that will be mapped */
  //all pointing to 0
  strcpy(filename, "papers.txt");
  strcpy(filename1, "papers.txt");
  strcpy(filename2, "papers.txt");
  /*strcpy(filename1, "papers.txt");*/
  //all ppage has been written to (swap)

  /* Map a page from the specified file */
  //these pages are now "infile"
  char* p = static_cast<char *>(vm_map (filename, 0));
  char* p1 = static_cast<char *>(vm_map (filename1, 0));

  /* Print the first part of the paper */
  for (unsigned int i=0; i<10; i++) {
    //need evicting(bring from file)
    //directly write to file
    p[i] ++;
    cout << p[i];
  }
  cout << '\n';
  for (unsigned int i=0; i<10; i++) {
    //already in memory
    cout << p1[i];
    p1[i] ++;
  }
  cout << '\n';
  for (unsigned int i=0; i<10; i++) {
    cout << p[i];
  }
}
