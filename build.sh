#!/bin/sh

if [ ! -x "$(which xmake)" ]; then
  .tools/bin/xmake $1
else
  xmake $1
fi
