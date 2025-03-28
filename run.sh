#!/bin/sh
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color
files=(test*.4)  # Store matching files in an array
shuffled=($(shuf -e "${files[@]}"))  # Shuffle them
for i in {0..5}; do
  printf "iter${RED}%d" i
for test in "${shuffled[@]}"; do
  printf "\n**${BLUE}$test${NC} running**\n"
  ./$test 
  wait
done
done
