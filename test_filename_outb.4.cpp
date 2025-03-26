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
  pid_t pid2 = fork();
if(pid2 == 0){
    char* filename33 = static_cast<char *>(vm_map(nullptr, 0));
    char* filename34 = static_cast<char *>(vm_map(nullptr, 0));
    strcpy(filename33+VM_PAGESIZE-4, "papers.txt");
    strcpy(filename34+VM_PAGESIZE-9, "papers.txt");
  char* pp = static_cast<char *>(vm_map (filename33+VM_PAGESIZE-4, 0));
    if(pp != nullptr) pp[0]++;
  char* p11 = static_cast<char *>(vm_map (filename34+VM_PAGESIZE-9, 1));
    std::cout << p11[0];
  } else if(pid != 0){
  char* filename = static_cast<char *>(vm_map(nullptr, 0));
  /* Write the name of the file that will be mapped */
  //all pointing to 0
  strcpy(filename+VM_PAGESIZE-4, "pa");
  /*strcpy(filename1, "papers.txt");*/
  //all ppage has been written to (swap)

  /* Map a page from the specified file */
  char* p = static_cast<char *>(vm_map (filename+VM_PAGESIZE, 0));

  /* Print the first part of the paper */
    //need evicting(bring from file)
    if(p!=nullptr)
    cout << p[0];
    //write fault
  } else {
    std::cout << "child process running:\n";
  /* Allocate swap-backed page from the arena */
  char* filename = static_cast<char *>(vm_map(nullptr, 0));
  char* filename1 = static_cast<char *>(vm_map(nullptr, 0));
  char* filename2 = static_cast<char *>(vm_map(nullptr, 0));
  filename = static_cast<char *>(vm_map(nullptr, 0));
  filename = static_cast<char *>(vm_map(nullptr, 0));
  filename = static_cast<char *>(vm_map(nullptr, 0));
  filename = static_cast<char *>(vm_map(nullptr, 0));
  filename = static_cast<char *>(vm_map(nullptr, 0));
  /* Write the name of the file that will be mapped */
  //all pointing to 0
  char x = filename[0];
  strcpy(filename, "papers.txt");
  strcpy(filename1, "papers.txt");
  strcpy(filename2, "papers.txt");
  /*strcpy(filename1, "papers.txt");*/
  //all ppage has been written to (swap)

  /* Map a page from the specified file */
  char* p = static_cast<char *>(vm_map (filename, 0));
  char* p1 = static_cast<char *>(vm_map (filename1, 0));

  /* Print the first part of the paper */
  for (unsigned int i=0; i<10; i++) {
    //need evicting(bring from file)
    cout << p[i];
    //write fault
    p[i] ++;
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

}
