#!/bin/bash

if ! [ -x "$(command -v emcc)" ]; then
  echo "Error: emcc could not be found." >&2
  exit 1
fi

# Common compiler flags
export CFLAGS="-fPIC $EXTRA_CFLAGS"
export CXXFLAGS="$CFLAGS -sNO_DISABLE_EXCEPTION_CATCHING $EXTRA_CXXFLAGS"
export LDFLAGS="-sEXPORTED_FUNCTIONS=_main,_malloc,_free -sALLOW_TABLE_GROWTH -sASSERTIONS -sNO_DISABLE_EXCEPTION_CATCHING -sWASM_BIGINT -Wno-main"

# Specific variables for cross-compilation
export CHOST="${TARGET_HOST}-unknown-linux" # wasm32-unknown-emscripten

autoreconf -fiv
emconfigure ./configure --prefix="$(pwd)/target" --host=$CHOST --enable-static --disable-shared \
  --disable-builddir --disable-multi-os-directory --disable-raw-api --disable-docs ${EXTRA_CONFIGURE_FLAGS} ||
  (cat config.log && exit 1)
make

EMMAKEN_JUST_CONFIGURE=1 emmake make check \
  RUNTESTFLAGS="LDFLAGS_FOR_TARGET='$LDFLAGS $EXTRA_TEST_LDFLAGS'" || (cat testsuite/libffi.log && exit 1)
