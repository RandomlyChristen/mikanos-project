#!/bin/bash -ex

if [ $# -ne 2 ]
then
    echo "Usage: $0 <MikanLoaderPkg dir> <output dir>"
    exit 1
fi

PKG_DIR=$1
OUT_DIR=$2
CWD=$PWD
DEVENV_DIR=$(dirname "$0")

cp $DEVENV_DIR/target.txt $HOME/edk2/Conf/target.txt
ln -sf $CWD/$PKG_DIR $HOME/edk2
cd $HOME/edk2
git checkout tags/edk2-stable202102

shift
shift

source edksetup.sh
build

cd $CWD
cp -r $HOME/edk2/Build/MikanLoaderX64 $OUT_DIR