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
#ifndef SRC_NODE_REVERT_H_
#define SRC_NODE_REVERT_H_

#include "node.h"

/**
 * Note that it is expected for this list to vary across specific LTS and
 * Stable versions! Only CVE's whose fixes require *breaking* changes within
 * a given LTS or Stable may be added to this list, and only with CTC
 * consensus.
 *
 * For *master* this list should always be empty!
 *
 **/
#define REVERSIONS(XX)                                                        \
  XX(CVE_2016_2216, "CVE-2016-2216", "Strict HTTP Header Parsing")

namespace node {

typedef enum {
#define V(code, _, __) REVERT_ ## code,
  REVERSIONS(V)
#undef V
} reversions_t;


/* A bit field for tracking the active reverts */
extern unsigned int reverted;

/* Revert the given CVE (see reversions_t enum) */
void Revert(const unsigned int cve);

/* Revert the given CVE by label */
void Revert(const char* cve);

/* true if the CVE has been reverted **/
bool IsReverted(const unsigned int cve);

/* true if the CVE has been reverted **/
bool IsReverted(const char * cve);

}  // namespace node

#endif  // SRC_NODE_REVERT_H_
