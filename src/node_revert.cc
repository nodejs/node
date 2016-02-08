// Copyright Node Contributors.
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
#include "node_revert.h"
#include <stdio.h>
#include <string.h>

namespace node {

unsigned int reverted = 0;

const char* RevertMessage(const unsigned int cve) {
#define V(code, label, msg) case REVERT_ ## code: return label ": " msg;
  switch (cve) {
    REVERSIONS(V)
    default:
      return "Unknown";
  }
#undef V
}

void Revert(const unsigned int cve) {
  reverted |= 1 << cve;
  printf("SECURITY WARNING: Reverting %s\n", RevertMessage(cve));
}

void Revert(const char* cve) {
#define V(code, label, _)                                                     \
  do {                                                                        \
    if (strcmp(cve, label) == 0) {                                            \
      Revert(static_cast<unsigned int>(REVERT_ ## code));                     \
      return;                                                                 \
    }                                                                         \
  } while (0);
  REVERSIONS(V)
#undef V
  printf("Error: Attempt to revert an unknown CVE [%s]\n", cve);
  exit(12);
}

bool IsReverted(const unsigned int cve) {
  return reverted & (1 << cve);
}

bool IsReverted(const char * cve) {
#define V(code, label, _)                                                     \
  do {                                                                        \
    if (strcmp(cve, label) == 0)                                              \
      return IsReverted(static_cast<unsigned int>(REVERT_ ## code));          \
  } while (0);
  REVERSIONS(V)
  return false;
#undef V
}

}  // namespace node