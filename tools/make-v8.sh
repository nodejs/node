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
    *clang*)
      CLANG_VERSION=$(clang -dumpversion | cut -d. -f1)
      RUST_VERSION=$(rustc --version --verbose | awk '/release:/ {print $2}')
      GN_COMPILER_OPTS="is_clang=true clang_base_path=\"/usr\" clang_use_chrome_plugins=false treat_warnings_as_errors=false use_custom_libcxx=false clang_version=\"${CLANG_VERSION}\""
      GN_RUST_ARGS="rustc_version=\"${RUST_VERSION}\" rust_sysroot_absolute=\"/usr\" rust_bindgen_root=\"/usr\""
      export RUSTC_BOOTSTRAP=1

      # Filter out options supported in clang >= 20 when using clang 19.
      if [ "${CLANG_VERSION}" -eq "19" ]; then
        find build/ \( -name "*.gn" -o -name "*.gni" \) -exec sed -i \
          -e '/-Wno-nontrivial-memcall/d' {} \;
      fi
      # Filter out compiler options not supported by non-nightly clang builds
      if [ "${CLANG_VERSION}" -ge "19" ]; then
        find build/ \( -name "*.gn" -o -name "*.gni" \) -exec sed -i \
          -e '/-Wno-unsafe-buffer-usage-in-static-sized-array/d' \
          -e '/-Wno-uninitialized-const-pointer/d' \
          -e '/-Wno-unused-but-set-global/d' \
          -e '/-fno-lifetime-dse/d' \
          -e '/-fsanitize-ignore-for-ubsan-feature/d' \
          -e '/-fdiagnostics-show-inlining-chain/d' {} \;
      fi
      # Patch to match libadler/libadler2 based on what is present.
      LIBADLER=$(basename "$(find "$(rustc --print sysroot)/lib/rustlib/" -type f -name "libadler*")" | cut -d '-' -f 1) || true
      case "$LIBADLER" in
        libadler2) sed -i -e 's/"adler"/"adler2"/g' build/rust/std/BUILD.gn
          ;;
        libadler) sed -i -e 's/"adler2"/"adler"/g' build/rust/std/BUILD.gn
          ;;
        *) echo "Warning: unable to detect libadler/libadler2 (found ${LIBADLER})"
          ;;
      esac
      ;;
    *) GN_COMPILER_OPTS="treat_warnings_as_errors=false use_custom_libcxx=false" ;;
  esac
  gn gen -v "out.gn/$BUILD_ARCH_TYPE" --args="$GN_COMPILER_OPTS is_component_build=false is_debug=false v8_target_cpu=\"$TARGET_ARCH\" target_cpu=\"$TARGET_ARCH\" v8_enable_backtrace=true ${GN_RUST_ARGS} $CC_WRAPPER"
  ninja -v -C "out.gn/$BUILD_ARCH_TYPE" "${JOBS_ARG}" d8 cctest inspector-test
else
  DEPOT_TOOLS_DIR="$(cd depot_tools && pwd)"
  # shellcheck disable=SC2086
  PATH="$DEPOT_TOOLS_DIR":$PATH tools/dev/v8gen.py "$BUILD_ARCH_TYPE" $V8_BUILD_OPTIONS
  PATH="$DEPOT_TOOLS_DIR":$PATH ninja -C "out.gn/$BUILD_ARCH_TYPE/" "${JOBS_ARG}" d8 cctest inspector-test
fi
