RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color
for test in test*.4
do
  printf "\n**${BLUE}$test${NC} running**\n"
  ./$test
  wait
done

