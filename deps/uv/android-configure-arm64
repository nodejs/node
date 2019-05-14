#!/bin/bash

export TOOLCHAIN=$PWD/android-toolchain-arm64
mkdir -p $TOOLCHAIN
API=${3:-24}
$1/build/tools/make-standalone-toolchain.sh \
    --toolchain=aarch64-linux-android-4.9 \
    --arch=arm64 \
    --install-dir=$TOOLCHAIN \
    --platform=android-$API \
    --force
export PATH=$TOOLCHAIN/bin:$PATH
export AR=aarch64-linux-android-ar
export CC=aarch64-linux-android-gcc
export CXX=aarch64-linux-android-g++
export LINK=aarch64-linux-android-g++
export PLATFORM=android
export CFLAGS="-D__ANDROID_API__=$API"

if [[ $2 == 'gyp' ]]
  then
    ./gyp_uv.py -Dtarget_arch=arm64 -DOS=android -f make-android
fi
