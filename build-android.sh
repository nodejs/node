#!/bin/bash

export toolchain='/home/a/android-ndk-r19c/toolchain'

git checkout base-12.2.0
./configure
make -j4
cp out/Release/torque .

git checkout 12.2.0-android

# rm -Rf out
./android-configure
make -j4

rm -f libnode.a
"$toolchain/bin/aarch64-linux-android-ar" -M <libnode.mri
