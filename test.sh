#!/usr/bin/env bash

DIR="test"

RED='\033[0;31m'
BLUE='\033[34m'
NC='\033[0m' # No Color

for file in "$DIR"/*.prr; do

    if ! ./build/release/purr "$file"; then
        echo -e "${RED}Error occurred while running $file${NC}"
    fi

    echo -e "${BLUE}\n\n--- end ---\n${NC}"
done
