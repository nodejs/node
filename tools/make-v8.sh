#!/bin/bash -xe

BUILD_ARCH_TYPE=$1
V8_BUILD_OPTIONS=$2

cd deps/v8
tools/node/fetch_deps.py .

ARCH="`arch`"
if [[ "$ARCH" == "s390x" ]] || [[ "$ARCH" == "ppc64le" ]]; then
  TARGET_ARCH=$ARCH
  if [[ "$ARCH" == "ppc64le" ]]; then
    TARGET_ARCH="ppc64"
  fi
  # set paths manually for now to use locally installed gn
  export BUILD_TOOLS=/home/iojs/build-tools
  export LD_LIBRARY_PATH=$BUILD_TOOLS:$LD_LIBRARY_PATH
  # Avoid linking to ccache symbolic links as ccache decides which
  # binary to run based on the name of the link (we always name them gcc/g++).
  CC_PATH=`which -a $CC gcc | grep -v ccache | head -n 1`
  CXX_PATH=`which -a $CXX g++ | grep -v ccache | head -n 1`
  rm -f "$BUILD_TOOLS/g++"
  rm -f "$BUILD_TOOLS/gcc"
  ln -s $CXX_PATH "$BUILD_TOOLS/g++"
  ln -s $CC_PATH "$BUILD_TOOLS/gcc"
  export PATH=$BUILD_TOOLS:$PATH

  g++ --version
  gcc --version
  export PKG_CONFIG_PATH=$BUILD_TOOLS/pkg-config
  gn gen -v out.gn/$BUILD_ARCH_TYPE --args="is_component_build=false is_debug=false use_goma=false goma_dir=\"None\" use_custom_libcxx=false v8_target_cpu=\"$TARGET_ARCH\" target_cpu=\"$TARGET_ARCH\" v8_enable_backtrace=true"
  ninja -v -C out.gn/$BUILD_ARCH_TYPE d8 cctest inspector-test
else
  PATH=~/_depot_tools:$PATH tools/dev/v8gen.py $BUILD_ARCH_TYPE --no-goma $V8_BUILD_OPTIONS
  PATH=~/_depot_tools:$PATH ninja -C out.gn/$BUILD_ARCH_TYPE/ d8 cctest inspector-test
fi
