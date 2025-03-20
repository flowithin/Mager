#include "vm_arena.h"
#include "vm_pager.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <iomanip>
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
/*#define LOG*/

struct PMB{bool ref, dirty;};
struct Clock{
  uint32_t ppage;
  bool free;
};
struct Pt{
  uint32_t size, numsw;
  page_table_entry_t* st;
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

static uint32_t pcnt;
static uint32_t blcnt;
static uint32_t pinned;
static uint32_t curr_pid;
static uint32_t bound;
static uint32_t eblcnt;
static std::queue<uint32_t> clock_q;
static std::queue<uint32_t> free_ppage;
static std::unordered_map<uint32_t, PMB> psuff;
static std::unordered_map<uint32_t, Pt> all_pt; //pid -> pt.
static std::unordered_map<page_table_entry_t*, Infile> infile;//map vpage -> filename
static std::unordered_map<std::string, std::map<uint32_t, fbp>> filemap;//filename -> (block, p.t.e)
static std::unordered_map<uint32_t, std::pair<std::string, uint32_t>> ghost;//ppage -> (filename, block)
static std::queue<uint32_t> free_block;
static std::unordered_map<uint32_t, std::set<page_table_entry_t*>> swfile;//swfile_block -> pte
static std::unordered_map<uint32_t, std::set<page_table_entry_t*>> core;
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

  std::cout << std::hex << t << "\n";
#endif
}

void p2p(uint32_t loc, char* content){
  if(content)
    std::memcpy(static_cast<char *>(vm_physmem) + loc * VM_PAGESIZE, content, VM_PAGESIZE);
  else 
    memset(static_cast<char *>(vm_physmem) + loc * VM_PAGESIZE, 0, VM_PAGESIZE);
}
/*template <typename k, typename t, typename v>*/
/*void hash_deep_erase(std::unordered_map<k, std::map<t, std::set<v>>> hm, k key1, t key2, v obj){*/
/*  hm[key1][key2].erase(obj);*/
/*  if(hm[key1][key2].empty())*/
/*    hm[key1][key2].erase()*/
/*}*/
int runclock(){
  while(1){
    clock_q.push(clock_q.front());
    clock_q.pop();
    if (psuff[clock_q.back()].ref){
      psuff[clock_q.back()].ref = 0;
      if(core.find(clock_q.back()) != core.end()){
        for(auto pte : core[clock_q.back()]){
          pte->read_enable = pte->write_enable = 0;
        }
      }
    } else{ 
      //the next one is at tail with ref 1
      psuff[clock_q.back()].ref = 1;
      return clock_q.back();
    }
  }
  return -1;
}
void vm_init(unsigned int memory_pages, unsigned int swap_blocks){
  pcnt = memory_pages;
  eblcnt = blcnt = swap_blocks;
  pinned = 0;//may not be valid
  //pinned page init to zeros
  p2p(0, nullptr);
  for(uint32_t i = 1; i < memory_pages; i ++){
    clock_q.push(i);
    free_ppage.push(i);
    psuff[i].ref = psuff[i].dirty = 0;
  }
  for(uint32_t i = 0; i < swap_blocks; i ++){
    free_block.push(i);
  }
}

int vm_create(pid_t parent_pid, pid_t child_pid){
  auto it = all_pt.find(parent_pid);
  uint32_t size = 0;
  uint32_t numsw = 0;
  page_table_entry_t* child_pt = new page_table_entry_t[VM_ARENA_SIZE]; 
  if(it != all_pt.end())
  {
    /*std::cout << "here\n";*/
    //parent_pid exists
    page_table_entry_t* parent_pt = it->second.st;
    assert(!free_block.empty());
    if (it->second.numsw > free_block.size()) {
      return -1;
    }
    size = it->second.size;
    for(size_t i = 0; i < size; i++){
      //cow
      parent_pt[i].write_enable = 0;
      //duplicate
      child_pt[i] = parent_pt[i];
      //update core
      core[child_pt[i].ppage].insert(child_pt+i);
      //update infile
      auto _it = infile.find(parent_pt+i);
      assert(_it != infile.end());
      if (_it->second.ftype == file_t::FILE_B)
      {
        infile[child_pt+i] = {_it->second.ftype, _it->second.infile, _it->second.block, _it->second.filename};
        //update filemap
        filemap[_it->second.filename][_it->second.block].vpset.insert(child_pt+i);
      } else {
        infile[child_pt+i] = {_it->second.ftype, _it->second.infile, free_block.front(), _it->second.filename};
        swfile[free_block.front()].insert(child_pt+i);
        free_block.pop();
        eblcnt--;
      }
    }
    numsw = it->second.numsw;
  }//if it not end
  //empty the arena (just a try)
  for(size_t i = size; i < VM_ARENA_SIZE / VM_PAGESIZE; i++){
    child_pt[i].ppage = child_pt[i].write_enable = child_pt[i].read_enable = 0;
  }
  eblcnt -= numsw;
  all_pt[child_pid] = Pt({size, numsw, child_pt});
  /*if(it != all_pt.end() && it->second.size != 0)*/
  /*  assert(it->second.st[it->second.size-1].ppage == all_pt[child_pid].st[all_pt[child_pid].size - 1].ppage);*/
  return 0;
}

void vm_switch(pid_t pid){
  curr_pid = pid;
  bound = all_pt[pid].size;
  page_table_base_register = all_pt[pid].st;
  /*std::cout<<"exited switch\n";*/
}



int pm_evict(){
  myPrint("pm_evict", "");
  int ppage = runclock();
  //no one can be evicted
  if (ppage == -1) return -1;
  if(ghost.find(ppage) != ghost.end()){
    //ghost page
    if(psuff[ppage].dirty)
      file_write(ghost[ppage].first.c_str(), ghost[ppage].second, (char*)vm_physmem + ppage * VM_PAGESIZE);
    filemap[ghost[ppage].first].erase(ghost[ppage].second);
    if(filemap[ghost[ppage].first].empty())
      filemap.erase(ghost[ppage].first);
    ghost.erase(ppage); 
    return ppage;
  }
  myPrint("core: ", "");
  print_map(core);
  assert(core.find(ppage) != core.end());
  //randomly choose one to see if they are in file
  page_table_entry_t* pte = *core[ppage].begin();
  myPrint("ppage = ", ppage);
  myPrint("pte = ", pte);
  auto _it = infile.find(pte);
  assert(_it != infile.end());
  assert(free_block.size() < blcnt);
  const char* filename;
  if(_it->second.ftype == file_t::SWAP)
     filename = nullptr;
  else
    filename = _it->second.filename.c_str();
  uint32_t block = infile[pte].block;
  if(psuff[ppage].dirty){
  //write back if dirty
    void* eaddr = (char*)(vm_physmem) + ppage * VM_PAGESIZE;
    file_write(filename, block, eaddr);
    //clear the dirty bit
    psuff[ppage].dirty = 0;
  }
    /*std::cout << "here\n";*/
    myPrint("filemap:", "");
    print_map(filemap);
  //downward all pte
  if(_it->second.ftype == file_t::FILE_B){
    //not in physmem anymore
    for(auto v : filemap[filename][_it->second.block].vpset){
      v->write_enable = v->read_enable = 0;
      infile[v].infile = true;
    }
  } else {
    for(auto v : swfile[block]){
      v->write_enable = v->read_enable = 0;
      infile[v].infile = true;
    }
  }
  //update core
  core.erase(ppage);
  return ppage;
}

int alloc(){
  //return a usable ppage
  int ppage;
  myPrint("clock_q: ", "");
  print_queue(clock_q);
  if (!free_ppage.empty()){
    myPrint("a free page!: ", "");
    ppage = free_ppage.front();
    free_ppage.pop();
    //want the clock to be aware of the new ppage
    for(size_t i = 0; i < pcnt - 1; i++){
      if (clock_q.front() != ppage){
        clock_q.push(clock_q.front());
        /*psuff[clock_q.front()].ref = 0;*/
        /*if(core.find(clock_q.front()) != core.end()){*/
        /*  for(auto pte : core[clock_q.front()]){*/
        /*    pte->read_enable = pte->write_enable = 0;*/
        /*  }*/
        /*}*/
      }
      clock_q.pop();
    }
    clock_q.push(ppage);
    psuff[ppage].ref = 1;
  } else {
    //need evicting
    ppage = pm_evict();
    if (ppage == -1) return -1;
  }
  myPrint("clock_q: ", "");
  print_queue(clock_q);
  myPrint("alloc: ", ppage);
  return ppage;
}
int cow(page_table_entry_t* pte, char* content){
  //copy on write
  /*std::cout << "cow\n";*/
  //allocate one ppage for it
  myPrint("cow\n", "");
  int ppage = alloc();
  if(ppage == -1) return -1;
  //write the read value to the new loc
  p2p(ppage, content);
  //update core
  core[pte->ppage].erase(pte);
  if(core[pte->ppage].empty()) core.erase(pte->ppage);
  pte->ppage = ppage;
  core[pte->ppage].insert(pte);
  //update ref and dirty and write read
  //update infile
  /*if (infile[pte].ftype == file_t::SWAP) {*/
    //swapfile
    /*swfile[infile[pte].block].erase(pte);*/
    /*swfile[infile[pte].block].insert(pte);*/
    //give a new swap block
    /*infile[pte].block = free_block.front();*/
    /*free_block.pop();*/
  /*}*/
  //dirty bit = 1
  psuff[pte->ppage].dirty = 1;
  pte->write_enable = 1;

  return 0;
}

bool is_cow(page_table_entry_t* pte){
  //pte should exist in core and also im mem
  return (core.find(pte->ppage) != core.end() && core[pte->ppage].size() > 1 && infile[pte].ftype == file_t::SWAP && !infile[pte].infile) || pte->ppage == pinned;
}
int vm_fault(const void *addr, bool write_flag){
  myPrint("core map: ", "");
  print_map(core);
  uint64_t page = (reinterpret_cast<uint64_t>(addr) - reinterpret_cast<uint64_t>(VM_ARENA_BASEADDR)) >> 16;
  if (page >= bound) {
    myPrint("page = ", page);
    myPrint("bound = ", bound);
    return -1;
  }
  page_table_entry_t* pte = page_table_base_register + page;
  auto it = infile.find(pte);
  assert(it != infile.end());
  if (it->second.infile){
    //in file, bring back
    int epage = alloc();
    if (epage == -1) return -1;
    void* eaddr =static_cast<char*>(vm_physmem) + epage * VM_PAGESIZE;
    myPrint("epage: ", epage);
    /*std::cout << "vpage 1: " << (*core[1].begin())->read_enable << (*core[1].begin())->write_enable << '\n';*/
    if(file_read(it->second.ftype == file_t::FILE_B ? it->second.filename.c_str() : nullptr, it->second.block, eaddr) == -1)
      return -1;
    //others lifted 
    auto& lifted = (it->second.ftype == file_t::SWAP)
      ? swfile[it->second.block]
      : filemap[it->second.filename][it->second.block].vpset;
    for(auto l: lifted){
      infile[l].infile = false;
      l->ppage = epage;
      l->read_enable = 1;
      //core map insert
      core[epage].insert(l);
    }
    if(it->second.ftype == file_t::FILE_B)
      filemap[it->second.filename][it->second.block].ppage = epage;
  }//if infile
  auto it_psuff = psuff.find(pte->ppage);
  if(it_psuff != psuff.end()){
    it_psuff->second.ref = 1;
    if(it_psuff->second.dirty)
      pte->write_enable = 1;//if dirty no need to set dirty
  }//except for pinned page
  pte->read_enable = 1;
  bool _is_cow = is_cow(pte);
  if (write_flag)
  {
    if (_is_cow){
      //copy on write
      if(cow(pte, static_cast<char *>(vm_physmem) + pte->ppage * VM_PAGESIZE) == -1) 
        return -1;
    } else {
      assert(it_psuff != psuff.end());//shouldn't be pinned page
      it_psuff->second.dirty = 1;
      //pte->ppage should be updated by now
      for(auto p : core[pte->ppage]){
        p->write_enable = 1;
      }
    }
  }
  myPrint("core map: ", "");
  print_map(core);

  return 0;
}


uint32_t a2p(const char* addr){
  /*myPrint("addr = ", reinterpret_cast<uintptr_t>(addr));*/
  /*myPrint("return = ", (reinterpret_cast<uintptr_t>(addr) - reinterpret_cast<uintptr_t>(VM_ARENA_BASEADDR)) >> 16);*/
  return (reinterpret_cast<uintptr_t>(addr) - reinterpret_cast<uintptr_t>(VM_ARENA_BASEADDR)) >> 16;
}
char mem(const char* addr){
  return static_cast<char *>(vm_physmem)[page_table_base_register[a2p(addr)].ppage * VM_PAGESIZE + (reinterpret_cast<uintptr_t>(addr) & 0xFFFF)];
}
std::string vm_to_string(const char *filename){
  uint32_t vpage = a2p(filename);
  /*uint32_t offset = reinterpret_cast<uintptr_t>(filename)& 0xFFFF;*/
  uint32_t i=0;
  std::string rs;
  while(1){
    //trigger fault if not in  arena
    if(page_table_base_register[vpage].read_enable == 0 && vm_fault(VM_ARENA_BASEADDR + vpage * VM_PAGESIZE, 0) == -1)
      return "@FAULT";
    if (mem(filename + i) == '\0')
      break;
    //the string we want to read
    rs += mem(filename + i++);
    vpage = a2p(filename + i);
  }
  /*myPrint("vm_to_string", "");*/

  return rs;
}
void* vm_map(const char *filename, unsigned int block){
  if(filename == nullptr && free_block.empty()){
    //swap file backed
      /*std::cerr << "free_block is empty\n";*/
      return nullptr;
  }
  page_table_entry_t* new_entry = page_table_base_register + all_pt[curr_pid].size;
  //file-backed
  if(filename != nullptr){
    std::string file_str = vm_to_string(filename);
    if(file_str == "@FAULT") return nullptr;
    myPrint("file_str = ", file_str);
    auto it = filemap.find(file_str);
    if (it != filemap.end()){
      //file matched
      auto _it = it->second.find(block);
      if(_it != it->second.end()){
        //block matched
        myPrint("matched! ","");
        new_entry->ppage = _it->second.ppage;
        if(ghost.find(_it->second.ppage) != ghost.end()){
          ghost.erase(_it->second.ppage);
          // NOTE: not sure
          // to set ref
          new_entry->read_enable = psuff[_it->second.ppage].ref;
          new_entry->write_enable = psuff[_it->second.ppage].ref & psuff[_it->second.ppage].dirty;
        } else{
          *new_entry = **_it->second.vpset.begin();
        }
        //ghost -> not infile
        bool is_infile = (!_it->second.vpset.empty() && infile[*_it->second.vpset.begin()].infile);
        infile[new_entry] = Infile{file_t::FILE_B, is_infile, block, file_str};
        //update core only if in mem
        if(!is_infile)
          core[new_entry->ppage].insert(new_entry);
        _it->second.vpset.insert(new_entry);
      } else goto notmatched;
    } else {
    notmatched:
      filemap[file_str][block].vpset.insert(new_entry);
      *new_entry = {.ppage = pinned, .read_enable = 0, .write_enable = 0};
      infile[new_entry] = Infile{file_t::FILE_B, true, block, file_str};
    }
  } else {
    //swap-backed
    infile[new_entry] = {file_t::SWAP, false, free_block.front(), "@SWAP"};
    swfile[free_block.front()].insert(new_entry);
    free_block.pop(); 
    eblcnt--;
    *new_entry = {.ppage = pinned, .read_enable = 1, .write_enable = 0 };
    core[new_entry->ppage].insert(new_entry);
  }
  //update core
  bound = ++all_pt[curr_pid].size;
  return (char*)VM_ARENA_BASEADDR + (bound - 1) * VM_PAGESIZE;
}

void vm_discard(page_table_entry_t* pte){
  auto it = infile.find(pte);
  /*std::cout << "infile size: " << infile.size() << '\n';*/
  /*myPrint("infile: ", infile, print_map<page_table_entry_t*, Infile>);*/
  assert(it!=infile.end());
  //free the blocks used and write back
  switch (it->second.ftype) {
    case file_t::SWAP: {
      //the last one leaves free up the block
      if(swfile[it->second.block].size() == 1){
        free_block.push(it->second.block);
        swfile.erase(it->second.block);
        eblcnt++;
      } else {
        swfile[it->second.block].erase(pte);
      }
      break;
    }
    case file_t::FILE_B: {
      //deeply clean filemap
      filemap[it->second.filename][it->second.block].vpset.erase(pte);
      if(it->second.infile){
        if(filemap[it->second.filename][it->second.block].vpset.empty())
          filemap[it->second.filename].erase(it->second.block);
        if(filemap[it->second.filename].empty())
          filemap.erase(it->second.filename);
      }
      break;
    }
  }
  //in mem
  if(!it->second.infile)
  {
    if(it->second.ftype == file_t::FILE_B && core[pte->ppage].size() == 1)
      ghost[pte->ppage] = std::pair(it->second.filename, it->second.block);
    else if(core[pte->ppage].size() == 1 && pte->ppage != pinned)
    {
    //give back the page
      free_ppage.push(pte->ppage);
      psuff[pte->ppage].ref = psuff[pte->ppage].dirty = 0;
    }
    if(core[pte->ppage].size() > 1)
      core[pte->ppage].erase(pte);
    else core.erase(pte->ppage);
  }
  //update infile
  infile.erase(pte);
}

void vm_destroy(){
  myPrint("infile size: ", infile.size());
  print_map(infile);
  //free all physical mem
  myPrint("filemap: ","");
  print_map(filemap);
  myPrint("core: ","");
  print_map(core);
  myPrint("swfile: ","");
  print_map(swfile);
  for(size_t i = 0; i < all_pt[curr_pid].size; i++){
    vm_discard(page_table_base_register + i);
  }
  myPrint("core after discard: ","");
  print_map(core);
  myPrint("filemap: ","");
  print_map(filemap);
  myPrint("ghost: ","");
  print_map(ghost);
 
  //free page table
  delete[] all_pt[curr_pid].st;
  all_pt.erase(curr_pid);
  myPrint("free_block: ", "");
  print_queue(free_block);
  myPrint("eblcnt: ", eblcnt);
}

