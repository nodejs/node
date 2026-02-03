#!/bin/sh

set -xe

BUILD_ARCH_TYPE=$1
V8_BUILD_OPTIONS=$2

cd deps/v8 || exit
find . -type d -name .git -print0 | xargs -0 rm -rf
../../tools/v8/fetch_deps.py .

JOBS_ARG=
if [ "${JOBS}" ]; then
  JOBS_ARG="-j ${JOBS}"
fi

ARCH=$(arch)
if [ "$ARCH" = "s390x" ] || [ "$ARCH" = "ppc64le" ]; then
  TARGET_ARCH=$ARCH
  if [ "$ARCH" = "ppc64le" ]; then
    TARGET_ARCH="ppc64"
  fi
  # Propagate ccache to gn.
  # shellcheck disable=SC2154
  case "$CXX" in
    *ccache*) CC_WRAPPER="cc_wrapper=\"ccache\"" ;;
    *) ;;
  esac

  # shellcheck disable=SC2154
  case "$CXX" in
    *clang*) GN_COMPILER_OPTS="is_clang=true clang_base_path=\"/usr\" clang_use_chrome_plugins=false treat_warnings_as_errors=false use_custom_libcxx=false" ;;
    *) GN_COMPILER_OPTS="treat_warnings_as_errors=false use_custom_libcxx=false" ;;
  esac
  gn gen -v "out.gn/$BUILD_ARCH_TYPE" --args="$GN_COMPILER_OPTS is_component_build=false is_debug=false v8_target_cpu=\"$TARGET_ARCH\" target_cpu=\"$TARGET_ARCH\" v8_enable_backtrace=true $CC_WRAPPER"
  ninja -v -C "out.gn/$BUILD_ARCH_TYPE" "${JOBS_ARG}" d8 cctest inspector-test
else
  DEPOT_TOOLS_DIR="$(cd depot_tools && pwd)"
  # shellcheck disable=SC2086
  PATH="$DEPOT_TOOLS_DIR":$PATH tools/dev/v8gen.py "$BUILD_ARCH_TYPE" $V8_BUILD_OPTIONS
  PATH="$DEPOT_TOOLS_DIR":$PATH ninja -C "out.gn/$BUILD_ARCH_TYPE/" "${JOBS_ARG}" d8 cctest inspector-test
fi
