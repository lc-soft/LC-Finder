#!/bin/sh

rootdir=`pwd`
logfile=`pwd`/setup.log
tooldir=`pwd`/.tools
echo > $logfile
if [ ! -d ".repos" ]; then
  mkdir .repos
fi

if [ ! -d ".tools" ]; then
  mkdir .tools
fi

echo downloading tool ...
cd .repos

if [ ! -x "$(which xmake)" ]; then
  git clone https://github.com/waruqi/xmake.git --branch master --depth 1
  cd xmake
  echo installing tool ...
  ./install $tooldir >> $logfile
  cd ../
fi

echo installing dependencies ...
sudo apt-get -y install build-essential automake libtool pkg-config libsqlite3-dev libpng-dev libjpeg-dev libxml2-dev libfreetype6-dev libx11-dev

echo downloading LCUI ...
if [ -d "LCUI" ]; then
  cd LCUI
  git pull
else
  if [ -d "${rootdir}/../LCUI" ]; then
    ln -s "${rootdir}/../LCUI" ./LCUI
    cd LCUI
  elif [ ! -d "LCUI" ]; then
    git clone https://github.com/lc-soft/LCUI.git --branch master --depth 1
    cd LCUI
    if [ ! -f "configure" ]; then
      ./autogen.sh >> $logfile
    fi
    ./configure >> $logfile
  else
    cd LCUI
  fi
fi
echo building LCUI ...
make >> $logfile
cd ../

echo downloading lcui.css ...
if [ -d "lcui.css" ]; then
  cd lcui.css
  git pull
else
  if [ -d "${rootdir}/../lcui.css" ]; then
    ln -s "${rootdir}/../lcui.css" ./lcui.css
    cd lcui.css
  elif [ ! -d "lcui.css" ]; then
    git clone https://github.com/lc-ui/lcui.css.git --branch master
    cd lcui.css
    npm install
  else
    cd lcui.css
  fi
fi
echo building lcui.css ...
npm run build-bin build-font build-css >> $logfile
cd ../

echo downloading libyaml ...
if [ ! -d "libyaml" ]; then
  git clone https://github.com/yaml/libyaml.git
  cd libyaml
  if [ ! -f "configure" ]; then
    ./bootstrap >> $logfile
  fi
  ./configure >> $logfile
else
  cd libyaml
  git pull origin master
fi
echo building libyaml ...
make >> $logfile
cd ../

echo downloading unqlite ...
if [ ! -d "unqlite" ]; then
  git clone https://github.com/symisc/unqlite.git --branch master --depth 1
  cd unqlite
else
  cd unqlite
  git pull origin master
fi
cp unqlite.c ../../src/lib
cp unqlite.h ../../include
cd ../../

echo the building environment has been completed!
