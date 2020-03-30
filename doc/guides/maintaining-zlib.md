# Maintaining zlib

This copy of zlib comes from the Chromium team's zlib fork which incorporated
performance improvements not currently available in standard zlib.

## Updating zlib

Update zlib:
```shell
git clone https://chromium.googlesource.com/chromium/src/third_party/zlib
cp deps/zlib/zlib.gyp deps/zlib/win32/zlib.def deps
rm -rf deps/zlib zlib/.git
mv zlib deps/
mv deps/zlib.gyp deps/zlib/
mkdir deps/zlib/win32
mv deps/zlib.def deps/zlib/win32
sed -i -- 's_^#include "chromeconf.h"_//#include "chromeconf.h"_' deps/zlib/zconf.h
```

Check that Node.js still builds and tests.

It may be necessary to update deps/zlib/zlib.gyp if any significant changes have
occurred upstream.

## Committing zlib

Add zlib: `git add --all deps/zlib`

Commit the changes with a message like
```text
deps: update zlib to upstream d7f3ca9

Updated as described in doc/guides/maintaining-zlib.md.
```
