This directory contains code that accesses Android (hw)binder interfaces and is
dynamically loaded and used by traced_probes / perfetto command line client.
The code in this directory is built as a separate .so library and can depend on
on Android internals.

Block diagram:

```
+---------------+       +---------------------------------+
| traced_probes |- - -> | libperfetto_android_internal.so |
+---------------+  ^    +---------------+-----------------+
                   |                    |
                   |                    | [  Non-NDK libraries ]
                   |                    +-> libbase.so
                   |                    +-> libutils.so
                   |                    +-> libhidltransport.so
                   |                    +-> libhwbinder.so
                   |                    +-> android.hardware.xxx@2.0
                   |
                   + dynamically loaded on first use via dlopen()
```

The major reason for using a separate .so() and introducing the shared library
layer is avoiding the cost of linker relocations (~150 KB private dirty)
required for loading the graph of binder-related libraries.

The general structure and rules for code in this directory is as-follows:
- Targets herein defined must be leaf targets. Dependencies to perfetto targets
  (e.g. base) are not allowed, as doing that would create ODR violations.
- Headers (e.g. health_hal.h) must have a plain old C interface (to avoid
  dealing with name mangling) and should not expose neither android internal
  structure/types nor struct/types defined in perfetto headers outside of this
  directory.
- Dependencies to Android internal headers are allowed only in .cc files, not
  in headers.
