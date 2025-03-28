#include "vm_arena.h"
#include "vm_pager.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <memory>
#include <unordered_map>
#include <queue>
#include <iostream>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <set>
#include <map>
#include <string>
#include <system_error>
#include <unordered_set>
/*#define LOG2*/
/*#define LOG*/

struct PMB{bool ref, dirty;};
struct Clock{
  uint32_t ppage;
  bool free;
};
struct Pt{
  uint32_t size, numsw;
  std::unique_ptr<page_table_entry_t []> st;
};
enum class file_t{
  SWAP,
  FILE_B,
};
struct Infile{
  file_t ftype;
  bool infile;
  uint32_t block;
  std::string filename;
};
struct fbp{
  uint32_t ppage;
  std::set<page_table_entry_t*> vpset;
};
std::ostream& operator<<(std::ostream& os, const Infile& inf){
  switch (inf.ftype) {
    case file_t::SWAP:
      {
        os << "ftype: SWAP\n";
        break;
      }
    case file_t::FILE_B:
      {
        os << "ftype: FILE_B\n";
        break;
      }
  }
  if(inf.infile)
    os << "infile: true\n";
  else os<< "infile: false\n";
  os << "block: " << inf.block << '\n' << "filename: " << inf.filename << '\n';
  return os;
}
template <typename t>
std::ostream& operator<<(std::ostream& os, const std::set<t>& set){
std::cout << "[";
  int i = 0;
  for(auto elt : set){
    i++;
    std::cout << elt ;
    if(i < set.size())
      std::cout << ", ";
  }
  std::cout << "]";
  return os;
}
template <typename k, typename t>
std::ostream& operator<<(std::ostream& os, const std::map<k, t>& map){
std::cout << "[";
  int i = 0;
  for(auto elt : map){
    i++;
    std::cout << "(" << elt.first << " -> "<< elt.second<< ")";
    if(i < map.size())
      std::cout << ", ";
  }
  std::cout << "]";
  return os;
}
std::ostream& operator<<(std::ostream& os, const fbp f){
  std::cout << "("<< f.ppage << ", " << f.vpset << ")";
  return os;
}


template <typename k, typename t>
std::ostream& operator<<(std::ostream& os, const std::pair<k, t> p){
  std::cout << "("<< p.first << ", " << p.second << ")";
  return os;
}
std::ostream& operator<<(std::ostream& os, const PMB pmb){
  std::cout << "("<< pmb.ref << ", " << pmb.dirty << ")";
  return os;
}
template <typename k, typename t>
void print_map(const std::unordered_map<k, t>& map){
  #ifdef LOG
  auto it = map.begin();
  while(it != map.end()){
    std::cout << "[" << it->first << " -> " << it->second << "]\n";
    it++;
  }
#endif
}
template <typename k, typename t>
void print_map(const std::map<k, t>& map){
  #ifdef LOG
  auto it = map.begin();
  while(it != map.end()){
    std::cout << "[" << it->first << " -> " << it->second << "]\n";
    it++;
  }
#endif
}
template <typename t>
void print_queue(std::queue<t> q){
#ifdef LOG
  std::cout << "(";
  for(size_t i = 0; i < q.size(); i++){
    std::cout << q.front() ;
    if(i < q.size() - 1)
      std::cout <<", ";
    q.push(q.front());
    q.pop();
  }
  std::cout << ")\n";
#endif
}


template <typename T, typename...>
void myPrint(std::string words, const T& t){
#ifdef LOG
  std::cout << words;
  /*if (func != nullptr)*/
  /*  *func(t);*/
  /*else */

  std::cout << std::hex << t << std::endl;
#endif
}
