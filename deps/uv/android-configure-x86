#!/bin/bash

export TOOLCHAIN=$PWD/android-toolchain-x86
mkdir -p $TOOLCHAIN
API=${3:-24}
$1/build/tools/make-standalone-toolchain.sh \
    --toolchain=x86-4.9 \
    --arch=x86 \
    --install-dir=$TOOLCHAIN \
    --platform=android-$API \
    --force
export PATH=$TOOLCHAIN/bin:$PATH
export AR=i686-linux-android-ar
export CC=i686-linux-android-gcc
export CXX=i686-linux-android-g++
export LINK=i686-linux-android-g++
export PLATFORM=android
export CFLAGS="-D__ANDROID_API__=$API"

if [[ $2 == 'gyp' ]]
  then
    ./gyp_uv.py -Dtarget_arch=x86 -DOS=android -f make-android
fi
