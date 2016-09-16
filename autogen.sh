#!/bin/sh

tooldir=`pwd`/.tools
if [ ! -d ".repos" ]; then
  mkdir .repos
fi

if [ ! -d ".tools" ]; then
  mkdir .tools
fi

echo downloading tool ...
cd .repos
if [ ! -d "xmake" ]; then
  git clone git@github.com:waruqi/xmake.git
  cd xmake
else
  cd xmake
  git pull origin master
fi
echo installing tool ...
./install $tooldir

echo downloading recipes ...
cd ../
if [ ! -d "unqlite" ]; then
  git clone https://github.com/symisc/unqlite.git
  cd unqlite
else
  cd unqlite
  git pull origin master
fi

echo copying unqlite source files ...
cp unqlite.c ../../src/lib
cp unqlite.h ../../include
cd ../../
echo the building environment has been completed!

