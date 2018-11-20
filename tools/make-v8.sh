#!/bin/bash

BUILD_ARCH_TYPE=$1
V8_BUILD_OPTIONS=$2

cd deps/v8
tools/node/fetch_deps.py .
PATH=~/_depot_tools:$PATH tools/dev/v8gen.py $BUILD_ARCH_TYPE --no-goma $V8_BUILD_OPTIONS
PATH=~/_depot_tools:$PATH ninja -C out.gn/$BUILD_ARCH_TYPE/ d8 cctest inspector-test
