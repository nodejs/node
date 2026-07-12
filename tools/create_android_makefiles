#!/bin/bash
# Run this script ONLY inside an Android build system
# and after you ran lunch command!

if [ -z "$ANDROID_BUILD_TOP" ]; then
    echo "Run lunch before running this script!"
    exit 1
fi

if [ -z "$1" ]; then
    ARCH="arm"
else
    ARCH="$1"
fi

if [ $ARCH = "x86" ]; then
    TARGET_ARCH="ia32"
else
    TARGET_ARCH="$ARCH"
fi

cd $(dirname $0)/..

./configure \
    --without-snapshot \
    --openssl-no-asm \
    --dest-cpu=$TARGET_ARCH \
    --dest-os=android

export GYP_GENERATORS="android"
export GYP_GENERATOR_FLAGS="limit_to_target_all=true"
GYP_DEFINES="target_arch=$TARGET_ARCH"
GYP_DEFINES+=" v8_target_arch=$TARGET_ARCH"
GYP_DEFINES+=" android_target_arch=$ARCH"
GYP_DEFINES+=" host_os=linux OS=android"
export GYP_DEFINES

./deps/npm/node_modules/node-gyp/gyp/gyp \
    -Icommon.gypi \
    -Iconfig.gypi \
    --depth=. \
    -Dcomponent=static_library \
    -Dlibrary=static_library \
    node.gyp

echo -e "LOCAL_PATH := \$(call my-dir)\n\ninclude \$(LOCAL_PATH)/GypAndroid.mk" > Android.mk
