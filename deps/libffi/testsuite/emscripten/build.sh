#!/usr/bin/env bash

if ! [ -x "$(command -v emcc)" ]; then
  echo "Error: emcc could not be found." >&2
  exit 1
fi

set -e

SOURCE_DIR=$PWD

# Working directories
TARGET=$SOURCE_DIR/target
mkdir -p "$TARGET"

# Define default arguments
DEBUG=false

# Parse arguments
while [ $# -gt 0 ]; do
  case $1 in
    --debug) DEBUG=true ;;
    *) echo "ERROR: Unknown parameter: $1" >&2; exit 1 ;;
  esac
  shift
done

# Common compiler flags
export CFLAGS="-O3 -fPIC"
if [ "$DEBUG" = "true" ]; then export CFLAGS+=" -DDEBUG_F"; fi
export CXXFLAGS="$CFLAGS"

# Build paths
export CPATH="$TARGET/include"
export PKG_CONFIG_PATH="$TARGET/lib/pkgconfig"
export EM_PKG_CONFIG_PATH="$PKG_CONFIG_PATH"

# Specific variables for cross-compilation
export CHOST="${TARGET_HOST}-unknown-linux" # wasm32-unknown-emscripten

autoreconf -fiv
emconfigure ./configure --host=$CHOST --prefix="$TARGET" --enable-static --disable-shared --disable-dependency-tracking \
  --disable-builddir --disable-multi-os-directory --disable-raw-api --disable-docs ${EXTRA_CONFIGURE_FLAGS}
make install
cp fficonfig.h target/include/
cp include/ffi_common.h target/include/
