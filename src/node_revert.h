#ifndef SRC_NODE_REVERT_H_
#define SRC_NODE_REVERT_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

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
#define REVERSIONS(XX)
//  XX(CVE_2016_PEND, "CVE-2016-PEND", "Vulnerability Title")

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

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_REVERT_H_
