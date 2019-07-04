#!/bin/sh

mkdir -p bin
indent -kr -l210 -bli0 -di1 -i2 -bbo -nip -lp -nbad -nsai -bl -bls -nut  src/*.c src/*.h
gcc ./src/main.c -Ofast -pthread -llxi -lconfig -lncursesw -Wall -D_GNU_SOURCE -D_POSIX_PTHREAD_SEMANTICS -o ./bin/lxiidl 

