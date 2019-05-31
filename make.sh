#!/bin/sh

mkdir -p bin
gcc ./src/main.c -llxi -lconfig -lpthread -lncursesw -Wall -D_GNU_SOURCE -o ./bin/lxiidl 

