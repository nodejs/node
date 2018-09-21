#!/bin/bash

export TOOLCHAIN=$PWD/android-toolchain-x86_64
mkdir -p $TOOLCHAIN
API=${3:-24}
$1/build/tools/make-standalone-toolchain.sh \
    --toolchain=x86_64-4.9 \
    --arch=x86_64 \
    --install-dir=$TOOLCHAIN \
    --platform=android-$API \
    --force
export PATH=$TOOLCHAIN/bin:$PATH
export AR=x86_64-linux-android-ar
export CC=x86_64-linux-android-gcc
export CXX=x86_64-linux-android-g++
export LINK=x86_64-linux-android-g++
export PLATFORM=android
export CFLAGS="-D__ANDROID_API__=$API -fPIC"
export CXXFLAGS="-D__ANDROID_API__=$API -fPIC"
export LDFLAGS="-fPIC"

if [[ $2 == 'gyp' ]]
  then
    ./gyp_uv.py -Dtarget_arch=x86_64 -DOS=android -f make-android
fi
