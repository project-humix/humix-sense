#!/bin/bash

# check OS type

if [ "$(uname)" == "Darwin" ]; then
    OSTYPE=darwin
    ARCH=amd64
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then

    OSTYPE=linux
    if [ "$(expr substr $(uname -m) 1 3)" == "arm" ]; then
        ARCH=arm
    elif [ "$(expr substr $(uname -m) 1 6)" == "x86_64" ]; then
        ARCH=amd64
    fi
else
    OSTYPE=window
    ARCH=amd64
fi

BIN_PATH=bin/gnatsd
VER=0.94
NATSD=$BIN_PATH-$VER-$OSTYPE-$ARCH

echo "using natds executable:" $NATSD

# check existing process
killall $NATSD


$NATSD &
node index.js
