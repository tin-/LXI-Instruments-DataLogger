#!/bin/sh

mkdir -p bin
gcc ./src/main.c -llxi -lconfig -lpthread -lncursesw -Wall -o ./bin/lxiidl 

