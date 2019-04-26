#!/bin/sh


screen -r measurement
if [ $? -eq 0 ]
then
  exit 0
else
  screen -S measurement ./bin/lxiidl
  exit 0
fi
