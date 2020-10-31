ProtoZero
---------

*** note
**This doc is WIP**, stay tuned.
<!-- TODO(primiano): write protozero doc. -->
***

ProtoZero is an almost* zero-copy zero-malloc append-only protobuf library.
It's designed to be fast and efficient at the cost of a reduced API
surface for generated stubs. The main limitations consist of:
- Append-only interface: no readbacks are possible from the stubs.
- No runtime checks for duplicated or missing mandatory fields.
- Mandatory ordering when writing of nested messages: once a nested message is
  started it must be completed before adding any fields to its parent.

*** aside
Allocations and library calls will happen only when crossing the boundary of a
contiguous buffer (e.g., to request a new buffer to continue the write).
Assuming a chunk size (a trace *chunk* is what becomes a *contiguous buffer*
within ProtoZero) of 4KB, and an average event size of 32 bytes, only 7 out of
1000 events will hit malloc / ipc / library calls.
***


Other resources
---------------
* [Design doc](https://goo.gl/EKvEfa)
