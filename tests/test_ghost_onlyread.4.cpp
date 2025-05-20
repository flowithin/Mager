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
 /* Write the name of the file that will be mapped */
  //evict them all
  strcpy(filename, "papers.txt");
  strcpy(filename1, "papers.txt");
  strcpy(filename2, "papers.txt");
 

  char* p1 = static_cast<char *>(vm_map (filename1, 0));
  char* p2 = static_cast<char *>(vm_map (filename1, 0));
  char* p3 = static_cast<char *>(vm_map (filename1, 1));
  char* p4 = static_cast<char *>(vm_map (filename1, 2));

  /* Print the first part of the paper */
  for (unsigned int i=0; i<10; i++) {
    //need evicting(bring from file)
    cout << p1[i];
    //write fault
  }
  cout << '\n';
  for (unsigned int i=0; i<10; i++) {
    //already in memory
    cout << p3[i];
  }
 
  cout << '\n';
  for (unsigned int i=0; i<10; i++) {
    cout << p4[i];
  }
  //now (p, p1, p2); (p3, p4) are ghost pages
}
