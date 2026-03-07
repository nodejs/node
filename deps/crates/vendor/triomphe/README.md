# Triomphe

Fork of Arc. This has the following advantages over std::sync::Arc:

* `triomphe::Arc` doesn't support weak references: we save space by excluding the weak reference count, and we don't do extra read-modify-update operations to handle the possibility of weak references.
* `triomphe::UniqueArc` allows one to construct a temporarily-mutable `Arc` which can be converted to a regular `triomphe::Arc` later
* `triomphe::OffsetArc` can be used transparently from C++ code and is compatible with (and can be converted to/from) `triomphe::Arc`
* `triomphe::ArcBorrow` is functionally similar to `&triomphe::Arc<T>`, however in memory it's simply `&T`. This makes it more flexible for FFI; the source of the borrow need not be an `Arc` pinned on the stack (and can instead be a pointer from C++, or an `OffsetArc`). Additionally, this helps avoid pointer-chasing.
* `triomphe::Arc` can be constructed for dynamically-sized types via `from_header_and_iter`
* `triomphe::ThinArc` provides thin-pointer `Arc`s to dynamically sized types
* `triomphe::ArcUnion` is union of two `triomphe:Arc`s which fits inside one word of memory

This crate is a version of `servo_arc` meant for general community use.
