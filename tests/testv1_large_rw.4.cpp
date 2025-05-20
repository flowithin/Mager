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
  char x = filename[0];
  strcpy(filename,  "papers.txt");
  strcpy(filename1, "data1.bin");
  strcpy(filename2, "data2.bin");
  /*strcpy(filename1, "papers.txt");*/
  //all ppage has been written to (swap)

  /* Map a page from the specified file */
  char* p[3] = {static_cast<char *>(vm_map (filename, 0)), static_cast<char *>(vm_map (filename, 1)), static_cast<char *>(vm_map (filename, 2))};
   char* p1[3] = {static_cast<char *>(vm_map (filename1, 0)), static_cast<char *>(vm_map (filename1, 0)), static_cast<char *>(vm_map (filename1, 0))};
  char* p2[3] = {static_cast<char *>(vm_map (filename2, 0)), static_cast<char *>(vm_map (filename2, 1)), static_cast<char *>(vm_map (filename2, 2))};

  /* Print the first part of the paper */
    int i=0;
  for(int j = 0; j < 3;  j++){
  while(i < 10000) {
    //need evicting(bring from file)
    cout << p[j][i];
    //write fault
    p[j][i++] ++;
  }
  }
    i=0;
  for(int j = 0; j < 3;  j++){
  while(i < 10000) {
    //need evicting(bring from file)
    cout << p1[j][i];
    //write fault
    p2[j][i++] ++;
  }
  }
 }
