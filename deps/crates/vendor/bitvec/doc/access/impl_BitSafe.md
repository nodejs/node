# Read-Only Shared-Mutable Handle

This type marks a handle to a shared-mutable type that may be modified through
*other* handles, but cannot be modified through *this* one. It is used when a
[`BitSlice`] region has partial ownership of an element and wishes to expose the
entire underlying raw element to the user without granting them write
permissions.

Under the `feature = "atomic"` build setting, this uses `radium`’s best-effort
atomic alias; when this feature is disabled, it reverts to `Cell`.

[`BitSlice`]: crate::slice::BitSlice
