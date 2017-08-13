// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_NODE_VERSION_H_
#define SRC_NODE_VERSION_H_

#define NODE_MAJOR_VERSION 8
#define NODE_MINOR_VERSION 4
#define NODE_PATCH_VERSION 0

#define NODE_VERSION_IS_RELEASE 1

#ifndef NODE_STRINGIFY
#define NODE_STRINGIFY(n) NODE_STRINGIFY_HELPER(n)
#define NODE_STRINGIFY_HELPER(n) #n
#endif

#ifndef NODE_RELEASE
#define NODE_RELEASE "node"
#endif

#ifndef NODE_TAG
# if NODE_VERSION_IS_RELEASE
#  define NODE_TAG ""
# else
#  define NODE_TAG "-pre"
# endif
#else
// NODE_TAG is passed without quotes when rc.exe is run from msbuild
# define NODE_EXE_VERSION NODE_STRINGIFY(NODE_MAJOR_VERSION) "." \
                          NODE_STRINGIFY(NODE_MINOR_VERSION) "." \
                          NODE_STRINGIFY(NODE_PATCH_VERSION)     \
                          NODE_STRINGIFY(NODE_TAG)
#endif

# define NODE_VERSION_STRING  NODE_STRINGIFY(NODE_MAJOR_VERSION) "." \
                              NODE_STRINGIFY(NODE_MINOR_VERSION) "." \
                              NODE_STRINGIFY(NODE_PATCH_VERSION)     \
                              NODE_TAG
#ifndef NODE_EXE_VERSION
# define NODE_EXE_VERSION NODE_VERSION_STRING
#endif

#define NODE_VERSION "v" NODE_VERSION_STRING


#define NODE_VERSION_AT_LEAST(major, minor, patch) \
  (( (major) < NODE_MAJOR_VERSION) \
  || ((major) == NODE_MAJOR_VERSION && (minor) < NODE_MINOR_VERSION) \
  || ((major) == NODE_MAJOR_VERSION && \
      (minor) == NODE_MINOR_VERSION && (patch) <= NODE_PATCH_VERSION))

/**
 * Node.js will refuse to load modules that weren't compiled against its own
 * module ABI number, exposed as the process.versions.modules property.
 *
 * When this version number is changed, node.js will refuse
 * to load older modules.  This should be done whenever
 * an API is broken in the C++ side, including in v8 or
 * other dependencies.
 */
#define NODE_MODULE_VERSION 57 /* Node.js v8.0.0 */

#endif  // SRC_NODE_VERSION_H_
