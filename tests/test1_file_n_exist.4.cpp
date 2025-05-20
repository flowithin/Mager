#include <iostream>
#include <cstring>
#include <sched.h>
#include <unistd.h>
#include "vm_app.h"
#include "vm_arena.h"

using std::cout;

int main() {
  /* Allocate swap-backed page from the arena */
  pid_t pid = fork();
  if(pid != 0){
  char* filename = static_cast<char *>(vm_map(nullptr, 0));

  /* Write the name of the file that will be mapped */
  //all pointing to 0
  strcpy(filename, "papers1.txt");
  /*strcpy(filename1, "papers.txt");*/
  //all ppage has been written to (swap)

  /* Map a page from the specified file */
  char* p = static_cast<char *>(vm_map (filename, 0));
  char* p1 = static_cast<char *>(vm_map (filename, 1));
  char* p2 = static_cast<char *>(vm_map (filename, 2));

  /* Print the first part of the paper */
  for (unsigned int i=0; i<10; i++) {
    //need evicting(bring from file)
    cout << p[i];
    cout << p1[i];
    cout << p2[i];
    //write fault
    p[i] ++;
  }
 
  }
 if(pid == 0){
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

}
