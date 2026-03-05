#ifndef SRC_NODE_USDT_H_
#define SRC_NODE_USDT_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#if defined(NODE_NO_USDT)
// All USDT support explicitly disabled via --without-dtrace.
#define NODE_HAVE_USDT 0
#define NODE_DC_PUBLISH_ENABLED() (0)
#define NODE_DC_PUBLISH_PROBE(name, msg) do {} while (0)

#elif defined(NODE_HAVE_DTRACE)
// Tier 1: dtrace -h generated header.  On Linux (SystemTap wrapper) the
// header defines STAP_HAS_SEMAPHORES and a semaphore variable that starts
// at 0 and is incremented by an attached tracer — zero overhead without a
// tracer.  On macOS/FreeBSD/illumos (native DTrace) the header provides an
// is-enabled probe via NODE_DC_PUBLISH_ENABLED() — the kernel patches the
// probe site to a no-op when no tracer is attached.
#include "node_provider.h"

#define NODE_HAVE_USDT 1
// NODE_DC_PUBLISH_ENABLED() and NODE_DC_PUBLISH() come from node_provider.h.
// Alias NODE_DC_PUBLISH to NODE_DC_PUBLISH_PROBE for consistency with
// the _ENABLED/_PROBE naming convention used in call sites.
#define NODE_DC_PUBLISH_PROBE(name, msg) NODE_DC_PUBLISH((name), (msg))

#if defined(STAP_HAS_SEMAPHORES)
// Linux/SystemTap: real semaphore — JS can check without crossing into C++.
inline unsigned short* NodeDCPublishSemaphore() {
  return &node_dc__publish_semaphore;
}
#else
// macOS/FreeBSD/illumos: no semaphore variable — always report as enabled
// so that JS calls emitPublishProbe, which checks NODE_DC_PUBLISH_ENABLED()
// (the kernel is-enabled probe) and returns early if no tracer is attached.
inline unsigned short* NodeDCPublishSemaphore() {
  static unsigned short always_enabled = 1;
  return &always_enabled;
}
#endif

#elif defined(__has_include) && __has_include(<sys/sdt.h>)
// Tier 2: <sys/sdt.h> is available but dtrace -h was not used.  The
// semaphore is always 1 so the probe macro is always invoked; on DTrace
// platforms the probe site itself is a no-op until a tracer attaches, but
// the JS-to-C++ call overhead for emitPublishProbe is still incurred.
#include <sys/sdt.h>

#define NODE_HAVE_USDT 1

inline unsigned short* NodeDCPublishSemaphore() {
  static unsigned short always_enabled = 1;
  return &always_enabled;
}
#define NODE_DC_PUBLISH_ENABLED() (1)

#define NODE_DC_PUBLISH_PROBE(name, msg) \
  DTRACE_PROBE2(node, dc__publish, (name), (msg))

#else  // Tier 3: no dtrace, no <sys/sdt.h> — probes compile to no-ops

#define NODE_HAVE_USDT 0
#define NODE_DC_PUBLISH_ENABLED() (0)
#define NODE_DC_PUBLISH_PROBE(name, msg) do {} while (0)

#endif

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_USDT_H_
