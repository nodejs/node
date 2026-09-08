This directory contains partial re-implementations of some C++ standard library
utilities. They are for use with internal LLVM libc code and tests.

More utilities can be added on an as-needed basis. There are certain rules to
be followed for future changes and additions:

* Only certain kind of headers can be included:
   * Other headers from this directory
   * Free-standing C headers (`<stdint.h>` et al)
   * A few basic `src/__support/macros` headers used pervasively in all libc code

 * Free-standing C headers are to be included as C headers and not as C++
   headers. That is, use `#include <stddef.h>` and not `#include <cstddef>`.
   The [proxies](../../../hdr) can also be used, as in `#include "hdr/stdint_proxy.h"`.

* The utilities should be defined in the namespace `LIBC_NAMESPACE::cpp`. The
  higher level namespace should have a `__` prefix to avoid symbol name pollution
  when the utilities are used in implementation of public functions.

* Each "CPP/foo.h" provides an exact subset of the API from standard C++ <foo>,
  just using `LIBC_NAMESPACE::cpp::foo` names in place of `std::foo` names. The
  implementations here need not be perfect standard-conforming implementations,
  but their behavior must match for whatever _is_ supported at compile time.
  That is, if each were just declared with `using std::foo;` all the libc code
  should work the same (functionally speaking, excluding namespace entanglements).

* Additional APIs specific to libc internals belong elsewhere in `src/__support`,
  not in `src/__support/CPP`.
