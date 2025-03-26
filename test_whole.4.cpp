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
  pid_t pid1 = fork();
  /*std::cout << "pid = " << pid << '\n';*/
  /* Allocate swap-backed page from the arena */
  char* filename = static_cast<char *>(vm_map(nullptr, 0));
  char* filename1 = static_cast<char *>(vm_map(nullptr, 0));
  char* filename2 = static_cast<char *>(vm_map(nullptr, 0));
 /* Write the name of the file that will be mapped */
  //all pointing to 0
  strcpy(filename, "papers.txt");
  char* p;
  if(pid == 0){
    p = static_cast<char *>(vm_map (filename, 1));
    cout << ++p[0];
    p = static_cast<char *>(vm_map (filename, 2));
    cout << ++p[0];
    exit(0);
  }
  //these should evict all the pages
  strcpy(filename1, "papers.txt");
  strcpy(filename2, "papers.txt");
  strcpy(filename, "papers.txt");
  /*strcpy(filename1, "papers.txt");*/
  //all ppage has been written to (swap)

  /* Map a page from the specified file */
  p = static_cast<char *>(vm_map (filename, 0));
  char* p1 = static_cast<char *>(vm_map (filename1, 0));
  char* p2 = static_cast<char *>(vm_map (filename1, 0));
  char* p3 = static_cast<char *>(vm_map (filename1, 1));
  char* p4 = static_cast<char *>(vm_map (filename1, 2));

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
    cout << p3[i];
    p3[i] ++;
  }
 
  cout << '\n';
  for (unsigned int i=0; i<10; i++) {
    cout << p4[i];
    cout << p4[i];
  }
  //now (p, p1, p2); (p3, p4) are ghost pages
  if(pid != 0){
  strcpy(filename1, "papers.txt");
  strcpy(filename2, "papers.txt");
  strcpy(filename, "papers.txt");
  char* filename11 = static_cast<char *>(vm_map(nullptr, 0));
  char* filename12 = static_cast<char *>(vm_map(nullptr, 0));
  char* filename22 = static_cast<char *>(vm_map(nullptr, 0));
 /* Write the name of the file that will be mapped */
  //all pointing to 0
  strcpy(filename22, "papers.txt");
  char* p;
    if(pid1){
    p = static_cast<char *>(vm_map (filename, 1));
    cout << ++p[0];
    p = static_cast<char *>(vm_map (filename, 2));
    cout << ++p[0];
    }
 //these should evict all the pages
  strcpy(filename1, "papers.txt");
  strcpy(filename2, "papers.txt");
  /*strcpy(filename1, "papers.txt");*/
  //all ppage has been written to (swap)

  /* Map a page from the specified file */
  p = static_cast<char *>(vm_map (filename, 0));
  char* p11 = static_cast<char *>(vm_map (filename1, 0));
  char* p22 = static_cast<char *>(vm_map (filename1, 0));
  char* p33 = static_cast<char *>(vm_map (filename1, 1));
  char* p44 = static_cast<char *>(vm_map (filename1, 2));

    vm_yield();
  /* Print the first part of the paper */
  for (unsigned int i=0; i<10; i++) {
    //need evicting(bring from file)
    cout << p[i];
    //write fault
    p[i] ++;
  }
  strcpy(filename1, "papers.txt");
  strcpy(filename2, "papers.txt");
  //now should be infile rw=0
  char* p5 = static_cast<char *>(vm_map (filename11, 1));
  std::cout << p4[0];
  //now it is inmem rw=0
  char* p6 = static_cast<char *>(vm_map (filename12, 0));
  p6[0]++;
  //now is rw=1 inmem
  char* p7 = static_cast<char *>(vm_map (filename22, 0));
  std::cout << p5[0]++;

  cout << '\n';
  for (unsigned int i=0; i<10; i++) {
    //already in memory
    cout << p3[i];
    p3[i] ++;
  }
 
  cout << '\n';
  for (unsigned int i=0; i<10; i++) {
    cout << p4[i];
  }
  }
}
