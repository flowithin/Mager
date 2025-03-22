#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;

int main() {
  pid_t pid = fork();
  pid_t pid2 = fork();
  /* Allocate swap-backed page from the arena */
}
