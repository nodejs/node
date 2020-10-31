# Changes from Android 10

## New features
* Allow to specify whether profiling should only be done for existing processes
  or only for newly spawned ones using `no_startup` or `no_running` in
  `HeapprofdConfig`.
* Allow to get the number of bytes that were allocated at a callstack but then
  not used.
* Allow to dump the maximum, rather than at the time of the dump.

## Bugfixes
* Fixed heapprofd on x86.
* Fixed issue with calloc being incorrectly sampled.
