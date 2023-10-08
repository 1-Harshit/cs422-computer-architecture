#!/bin/bash

# if -l flag is set, then we will run local to remote sync
if [ "$1" == "-l" ]; then
    echo "Syncing local to remote"
    rsync -Pptrlzv /home/salazar/Documents/Courses/arch/pin-3.28-98749-g6643ecee5-gcc-linux/ cse1:/users/btech/harshitr/pin
fi

# if -r flag is set, then we will run remote to local sync
if [ "$1" == "-r" ]; then
    echo "Syncing remote to local"
    rsync -Pptrlzv cse1:/users/btech/harshitr/pin/ /home/salazar/Documents/Courses/arch/pin-3.28-98749-g6643ecee5-gcc-linux
fi

if [ "$1" == "-h" ]; then
    echo "Usage: ./sync.sh [OPTION]"
    echo "OPTION:"
    echo "  -l  local to remote sync"
    echo "  -r  remote to local sync"
    echo "  -h  help"
fi
