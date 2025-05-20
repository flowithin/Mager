#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;

int main() {
  pid_t pid = fork();
  pid_t pid2 = fork();
  /* Allocate swap-backed page from the arena */
  char* a = static_cast<char *>(vm_map(nullptr, 0));
  char* filename1 = static_cast<char *>(vm_map(nullptr, 0));
  char* filename2 = static_cast<char *>(vm_map(nullptr, 0));
  char* filename3 = static_cast<char *>(vm_map(nullptr, 0));
  a = static_cast<char *>(vm_map(nullptr, 0));
  /* Write the name of the file that will be mapped */
  //all pointing to 0
  //cow
  strcpy(a, "a first");
  strcpy(filename1, "papers.txt");
  strcpy(filename2, "papers.txt");
  strcpy(filename3, "papers.txt");
  strcpy(a, "a second");
  pid2 = fork();
  //now we have some vpages pointing to a non pinned ppage
  //also note that filename1's vpage is in file
  //the following should include copy on write
  for(int i =0 ; i < 10; i++){
    strcpy(a, "a first");
    //1.cow 
    //2. bring back from page evict filename2
    strcpy(filename1, "papers.txt");
     //1.cow 
    //2. bring back from page evict filename3
    strcpy(filename2, "papers.txt");
    //1.cow 
    //2. bring back from page evict a
    strcpy(filename3, "papers.txt");
    //read from sw file and write
    strcpy(a, "a second");
    vm_yield();
  }
}
