#!/bin/bash

workspace=$(realpath "$(dirname "$0")")

if [ $# -eq 0 ]; then
    CMD="west build -b seeeduino_xiao application/app/"
else     
    CMD="$*"
fi


docker run --rm -ti \
    -v $workspace:$workspace \
    ghcr.io/zephyrproject-rtos/zephyr-build:v0.28.7 \
     bash -c "cd $workspace && $CMD"
