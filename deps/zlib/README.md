This copy of zlib comes from the Chromium team's zlib fork which incorporated performance improvements not currently available in standard zlib.

To update this code:

* Clone https://chromium.googlesource.com/chromium/src/third_party/zlib
* Comment out the `#include "chromeconf.h"` in zconf.h to maintain full compatibility with node addons
