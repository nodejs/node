#ifndef SRC_NODE_USDT_H_
#define SRC_NODE_USDT_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

// USDT probe support for diagnostics_channel.
//
// Tier 1, Linux (on by default): the probe header is pre-generated with
// the SystemTap `dtrace` wrapper and committed at
// src/node_provider_linux.h (regenerate it with
// tools/usdt/generate_headers.py), so Linux builds never need a `dtrace`
// tool at build time.  This tier is used automatically whenever
// <sys/sdt.h> is available (systemtap-sdt-dev on Debian/Ubuntu,
// systemtap-sdt-devel on Fedora/RHEL).  It has effectively zero overhead
// when no tracer is attached: the semaphore check is a single memory
// load on the JS side and the probe site is a no-op until a tracer
// patches it.
//
// Tier 1, macOS (opt-in via ./configure --with-dtrace): the header is
// generated at build time with `dtrace -h` (always present with
// Xcode/CLT).  The kernel patches the probe sites to no-ops when no
// tracer is attached, but the JS-to-C++ call for emitPublishProbe() is
// still incurred on every publish, so this tier is opt-in.
//
// Tier 3 (everything else, or --without-dtrace): probes compile to
// no-ops with zero runtime overhead.
//
// FreeBSD/illumos are not supported yet: native DTrace there requires a
// `dtrace -G` link step that is not implemented.

// Everything in this header is declared at global scope intentionally:
// it shims the dtrace-generated probe headers, which declare their
// symbols at global scope, and its main API is macros, which
// namespaces do not affect. NodeDCPublishSemaphore() stays global for
// the same reason: it hands out the address of one of those symbols
// (or an always-enabled stand-in on macOS), so it lives beside what
// it points at.

#if defined(NODE_NO_USDT)

// Tier 3: explicitly disabled via ./configure --without-dtrace.
#define NODE_HAVE_USDT 0
#define NODE_DC_PUBLISH_ENABLED() (0)
#define NODE_DC_PUBLISH_PROBE(name, msg)                                       \
  do {                                                                         \
  } while (0)

#elif defined(__linux__) && defined(__has_include) && __has_include(<sys/sdt.h>)

// Tier 1, Linux: committed SystemTap-generated header with semaphore
// support.  NODE_DC_PUBLISH_ENABLED() and NODE_DC_PUBLISH() come from
// node_provider_linux.h.  NODE_DC_PUBLISH is aliased to
// NODE_DC_PUBLISH_PROBE for consistency with the naming convention used
// in call sites.
#define NODE_HAVE_USDT 1
#define NODE_USDT_HAVE_SEMAPHORE 1

#include "node_provider_linux.h"

#define NODE_DC_PUBLISH_PROBE(name, msg) NODE_DC_PUBLISH((name), (msg))

// Real semaphore — JS can check it without crossing into C++.
inline unsigned short* NodeDCPublishSemaphore() {  // NOLINT(runtime/int)
  return &node_dc__publish_semaphore;
}

#elif defined(NODE_HAVE_DTRACE)

// Tier 1, macOS (opt-in --with-dtrace): build-time `dtrace -h` generated
// header.  NODE_DC_PUBLISH_ENABLED() and NODE_DC_PUBLISH() come from
// node_provider.h.
#define NODE_HAVE_USDT 1

#include "node_provider.h"

#define NODE_DC_PUBLISH_PROBE(name, msg) NODE_DC_PUBLISH((name), (msg))

// No semaphore variable — always report as enabled so that JS calls
// emitPublishProbe(), which checks NODE_DC_PUBLISH_ENABLED() (the kernel
// is-enabled probe) and returns early if no tracer is attached.
inline unsigned short* NodeDCPublishSemaphore() {  // NOLINT(runtime/int)
  static unsigned short always_enabled = 1;        // NOLINT(runtime/int)
  return &always_enabled;
}

#else  // Tier 3: no USDT support — probes compile to no-ops

#define NODE_HAVE_USDT 0
#define NODE_DC_PUBLISH_ENABLED() (0)
#define NODE_DC_PUBLISH_PROBE(name, msg)                                       \
  do {                                                                         \
  } while (0)

#endif

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_USDT_H_
