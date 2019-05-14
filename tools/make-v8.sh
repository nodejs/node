#!/bin/bash -xe

BUILD_ARCH_TYPE=$1
V8_BUILD_OPTIONS=$2

cd deps/v8
tools/node/fetch_deps.py .

ARCH="`arch`"
if [[ "$ARCH" == "s390x" ]] || [[ "$ARCH" == "ppc64le" ]]; then
  # set paths manually for now to use locally installed gn
  export BUILD_TOOLS=/home/iojs/build-tools
  export LD_LIBRARY_PATH=$BUILD_TOOLS:$LD_LIBRARY_PATH
  export PATH=$BUILD_TOOLS:$PATH
  CXX_PATH=`which $CXX |grep g++`
  rm -f "$BUILD_TOOLS/g++"
  rm -f "$BUILD_TOOLS/gcc"
fi
if [[ "$ARCH" == "s390x" ]]; then
  ln -s $CXX_PATH "$BUILD_TOOLS/g++"
  ln -s $CXX_PATH "$BUILD_TOOLS/gcc"
  g++ --version
  export PKG_CONFIG_PATH=$BUILD_TOOLS/pkg-config
  gn gen -v out.gn/$BUILD_ARCH_TYPE --args='is_component_build=false is_debug=false use_goma=false goma_dir="None" use_custom_libcxx=false v8_target_cpu="s390x" target_cpu="s390x"'
  ninja -v -C out.gn/$BUILD_ARCH_TYPE d8 cctest inspector-test
elif [[ "$ARCH" == "ppc64le" ]]; then
  ln -s /usr/bin/$CXX "$BUILD_TOOLS/g++"
  ln -s /usr/bin/$CC "$BUILD_TOOLS/gcc"
  g++ --version
  export PKG_CONFIG_PATH=$BUILD_TOOLS/pkg-config-files
  gn gen out.gn/$BUILD_ARCH_TYPE --args='is_component_build=false is_debug=false use_goma=false goma_dir="None" use_custom_libcxx=false v8_target_cpu="ppc64" target_cpu="ppc64"'
  ninja -C out.gn/$BUILD_ARCH_TYPE d8 cctest inspector-test
else
  PATH=~/_depot_tools:$PATH tools/dev/v8gen.py $BUILD_ARCH_TYPE --no-goma $V8_BUILD_OPTIONS
  PATH=~/_depot_tools:$PATH ninja -C out.gn/$BUILD_ARCH_TYPE/ d8 cctest inspector-test
fi
