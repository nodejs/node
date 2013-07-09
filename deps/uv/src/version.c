/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"

 /*
 * Versions with an even minor version (e.g. 0.6.1 or 1.0.4) are API and ABI
 * stable. When the minor version is odd, the API can change between patch
 * releases. Make sure you update the -soname directives in config-unix.mk
 * and uv.gyp whenever you bump UV_VERSION_MAJOR or UV_VERSION_MINOR (but
 * not UV_VERSION_PATCH.)
 */

#undef UV_VERSION_MAJOR   /* TODO(bnoordhuis) Remove in v0.11. */
#undef UV_VERSION_MINOR   /* TODO(bnoordhuis) Remove in v0.11. */

#define UV_VERSION_MAJOR 0
#define UV_VERSION_MINOR 10
#define UV_VERSION_PATCH 12
#define UV_VERSION_IS_RELEASE 1


#define UV_VERSION  ((UV_VERSION_MAJOR << 16) | \
                     (UV_VERSION_MINOR <<  8) | \
                     (UV_VERSION_PATCH))

#define UV_STRINGIFY(v) UV_STRINGIFY_HELPER(v)
#define UV_STRINGIFY_HELPER(v) #v

#define UV_VERSION_STRING_BASE  UV_STRINGIFY(UV_VERSION_MAJOR) "." \
                                UV_STRINGIFY(UV_VERSION_MINOR) "." \
                                UV_STRINGIFY(UV_VERSION_PATCH)

#if UV_VERSION_IS_RELEASE
# define UV_VERSION_STRING  UV_VERSION_STRING_BASE
#else
# define UV_VERSION_STRING  UV_VERSION_STRING_BASE "-pre"
#endif


unsigned int uv_version(void) {
  return UV_VERSION;
}


const char* uv_version_string(void) {
  return UV_VERSION_STRING;
}
