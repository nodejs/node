#!/bin/bash

export TOOLCHAIN=$PWD/android-toolchain
mkdir -p $TOOLCHAIN
$1/build/tools/make-standalone-toolchain.sh \
    --toolchain=arm-linux-androideabi-4.8 \
    --arch=arm \
    --install-dir=$TOOLCHAIN \
    --platform=android-9
export PATH=$TOOLCHAIN/bin:$PATH
export AR=arm-linux-androideabi-ar
export CC=arm-linux-androideabi-gcc
export CXX=arm-linux-androideabi-g++
export LINK=arm-linux-androideabi-g++
export PLATFORM=android

if [ $2 -a $2 == 'gyp' ]
  then
    ./gyp_uv.py -Dtarget_arch=arm -DOS=android
fi
