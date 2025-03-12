#include <iostream>
#include <unordered_map>
template <typename k, typename t>
void print_map(const std::unordered_map<k, t>& map){
  auto it = map.begin();
  while(it != map.end()){
    std::cout << it->first << "\n" << it->second << '\n';
    it++;
  }
}

template <typename T, typename...>
void myPrint(std::string words, const T& t, void (*func)(const T&) = nullptr){
std::cout << words;
#ifdef LOG
  if (func != nullptr)
    *func(t);
  else 
    std::cout << t << "\n";
#endif
}
int main(){
}
