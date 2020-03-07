#!/bin/sh

rootdir=`pwd`
builddir=$rootdir/setup
logfile=$builddir/output.log
repodir=$builddir/repos
installdir=$rootdir/vendor

if [ ! -d $builddir ]; then
  mkdir $builddir
fi
if [ ! -d $repodir ]; then
  mkdir $repodir
fi

echo "============================================" >> $logfile
echo "start at" `date` >> $logfile
echo "============================================" >> $logfile

echo "rootdir: ${rootdir}"
echo "logfile: ${logfile}"
echo "repodir: ${repodir}"
echo "installdir: ${installdir}"
echo ""

install_xmake() {
  if [ ! -x "$(which xmake)" ]; then
    echo installing xmake ...
    curl -fsSL https://xmake.io/shget.text > install_xmake.sh
    bash install_xmake.sh
    rm install_xmake.sh
    source ~/.xmake/profile
  fi
}

install_dependencies() {
  echo installing dependencies ...
  sudo apt-get -qq -y install build-essential automake libtool pkg-config libsqlite3-dev libpng-dev libjpeg-dev libxml2-dev libfreetype6-dev libx11-dev
}

install_lcui() {
  echo "============================================" >> $logfile
  echo "install LCUI" >> $logfile
  echo "============================================" >> $logfile
  cd $repodir
  if [ ! -d "LCUI" ]; then
    echo "downloading LCUI ..."
    wget https://github.com/lc-soft/LCUI/archive/develop.zip -O LCUI-develop.zip
    echo "extracting LCUI ..."
    unzip LCUI-develop.zip >> $logfile
    rm LCUI-develop.zip
    mv LCUI-develop LCUI
    cd LCUI
    ./autogen.sh >> $logfile
    echo "configuring LCUI ..."
    ./configure --prefix=$installdir >> $logfile
  else
    cd LCUI
  fi

  echo "building LCUI ..."
  make >> $logfile
  make install >> $logfile
  echo "LCUI have been installed"
  cd $repodir
}

install_lcdesign() {
  echo "============================================" >> $logfile
  echo "install lc-design" >> $logfile
  echo "============================================" >> $logfile
  cd $repodir
  if [ ! -d "lc-design" ]; then
    echo "downloading lc-design ..."
    wget https://github.com/lc-ui/lc-design/archive/develop.zip -O lc-design-develop.zip
    echo "extracting lc-design ..."
    unzip lc-design-develop.zip >> $logfile
    rm lc-design-develop.zip
    mv lc-design-develop lc-design
    cd lc-design
    npm install
    xmake config --linkdirs=$installdir/lib --includedirs=$installdir/include --project=.
  else
    cd lc-design
  fi
  echo "building lc-design ..."
  npm run build-css >> $logfile
  npm run build-font >> $logfile
  xmake build --project=. >> $logfile
  xmake install --installdir=$installdir --project=. >> $logfile
  cp -r $installdir/share/assets ${rootdir}/app/
  echo "lc-design have been installed"
  cd $repodir
}

install_darknetlib() {
  echo "============================================" >> $logfile
  echo "install darknetlib" >> $logfile
  echo "============================================" >> $logfile
  cd $repodir
  if [ ! -d "darknetlib" ]; then
    echo "downloading darknetlib ..."
    git clone https://github.com/lc-soft/darknetlib.git >> $logfile
    cd darknetlib
    git submodule init >> $logfile
    git submodule update >> $logfile
  else
    cd darknetlib
  fi
  echo "building darknetlib ..."
  make >> $logfile
  cp libdarknet.so $installdir/lib
  cp include/*.h $installdir/include
  echo "darknetlib have been installed"
  cd $repodir
}

install_libyaml() {
  echo "============================================" >> $logfile
  echo "install libyaml" >> $logfile
  echo "============================================" >> $logfile
  cd $repodir
  if [ ! -d "libyaml" ]; then
    echo "downloading libyaml ..."
    git clone https://github.com/yaml/libyaml.git
    cd libyaml
    if [ ! -f "configure" ]; then
      ./bootstrap >> $logfile
    fi
    ./configure --prefix=$installdir >> $logfile
  else
    cd libyaml
  fi
  echo "building libyaml ..."
  make >> $logfile
  make install >> $logfile
  echo "libyaml have been installed"
  cd $repodir
}

install_unqlite() {
  echo "============================================" >> $logfile
  echo "install unqlite" >> $logfile
  echo "============================================" >> $logfile
  cd $repodir
  if [ ! -d "unqlite" ]; then
    echo "downloading unqlite ..."
    git clone https://github.com/symisc/unqlite.git --branch master --depth 1
    cd unqlite
  else
    cd unqlite
  fi
  cp unqlite.c $rootdir/src/lib
  cp unqlite.h $installdir/include
  echo "unqlite have been installed"
  cd $repodir
}

install_dependencies
install_xmake
install_lcui
install_lcdesign
install_libyaml
install_unqlite
install_darknetlib

cd $rootdir
if [ ! -d "app/lib" ]; then
  mkdir app/lib
fi
cp vendor/lib/*.so.* app/lib
cp vendor/lib/*.so app/lib

echo the building environment has been completed!
