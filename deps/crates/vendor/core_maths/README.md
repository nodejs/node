<!-- cargo-rdme start -->

Extension trait for full float functionality in `#[no_std]` backed by [`libm`].

Method signatures, implementation, and documentation are copied from as `std` 1.72,
with calls to instrinsics replaced by their `libm` equivalents.

# Usage
```rust
#[allow(unused_imports)] // will be unused on std targets
use core_maths::*;

3.9.floor();
```

<!-- cargo-rdme end -->
