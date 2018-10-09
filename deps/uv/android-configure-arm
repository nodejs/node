#!/bin/bash

export TOOLCHAIN=$PWD/android-toolchain-arm
mkdir -p $TOOLCHAIN
API=${3:-24}
$1/build/tools/make-standalone-toolchain.sh \
    --toolchain=arm-linux-androideabi-4.9 \
    --arch=arm \
    --install-dir=$TOOLCHAIN \
    --platform=android-$API \
    --force
export PATH=$TOOLCHAIN/bin:$PATH
export AR=arm-linux-androideabi-ar
export CC=arm-linux-androideabi-gcc
export CXX=arm-linux-androideabi-g++
export LINK=arm-linux-androideabi-g++
export PLATFORM=android
export CFLAGS="-D__ANDROID_API__=$API"

if [[ $2 == 'gyp' ]]
  then
    ./gyp_uv.py -Dtarget_arch=arm -DOS=android -f make-android
fi
