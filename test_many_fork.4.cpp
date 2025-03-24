#include <cstdlib>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"
#include "vm_arena.h"

using std::cout;

int main() {
  pid_t pid = fork();
  vm_yield();
  pid_t pid2 = fork();
  vm_yield();
  pid_t pid3 = fork();
  vm_yield();
  pid_t pid4 = fork();
  vm_yield();
  pid_t pid5 = fork();
  vm_yield();
  /* Allocate swap-backed page from the arena */
  char* a = static_cast<char *>(vm_map(nullptr, 0));
  strcpy(a, "a first");
  std::cout << a + VM_PAGESIZE;
  /* Write the name of the file that will be mapped */
  //all pointing to 0
  //cow

  for(int i =0 ; i < 10; i++){
    strcpy(a, "a first");
    //1.cow 
    //2. bring back from page evict filename2
    strcpy(a, "a second");
    vm_yield();
  }
}
