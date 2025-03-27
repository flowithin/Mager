#!/bin/sh
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color
files=(test*.4)  # Store matching files in an array
shuffled=($(shuf -e "${files[@]}"))  # Shuffle them
for test in "${shuffled[@]}"; do
  mv $test "testv1"
  wait
done
