<!-- Copyright 2025 The Fuchsia Authors

Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
<LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
This file may not be copied, modified, or distributed except according to
those terms. -->

# Unsafe Code Guidelines

`unsafe` code is extremely dangerous and should be avoided unless absolutely
necessary. When it is absolutely necessary to write `unsafe` code, it should be
done extremely carefully.

This document covers guidelines for writing unsafe code, including pointer casts
and safety comments.

## Pointer Casts

- **Avoid `&slice[0] as *const T`**: Use `slice.as_ptr()`. Accessing subsequent
  elements via pointer arithmetic on a single-element pointer is UB.
  ```rust
  let slice = &[1, 2];

  // BAD: Derived from reference to single element.
  let ptr = &slice[0] as *const i32;
  // SAFETY: UB! `ptr` has provenance only for the first element.
  // Accessing `ptr.add(1)` is out of bounds for this provenance.
  let val = unsafe { *ptr.add(1) };

  // GOOD: Derived from the slice itself.
  let ptr = slice.as_ptr();
  // SAFETY: Safe because `ptr` has provenance for the entire slice.
  let val = unsafe { *ptr.add(1) };
  ```
- **Avoid converting `&mut T` to `*const T`**: This reborrows as a shared
  reference, restricting permissions. Cast `&mut T` to `*mut T` first.
  ```rust
  let mut val = 42;
  let r = &mut val;

  // BAD: `r as *const i32` creates a shared reborrow.
  // The resulting pointer loses write provenance.
  let ptr = r as *const i32 as *mut i32;
  // SAFETY: UB! Writing to a pointer derived from a shared reborrow.
  unsafe { *ptr = 0 };

  // GOOD: `r as *mut i32` preserves write provenance.
  let ptr = r as *mut i32;
  // SAFETY: Safe because `ptr` retains mutable provenance.
  unsafe { *ptr = 0 };
  ```

## Safety Comments

Every `unsafe` block must be documented with a `// SAFETY:` comment.
- **Requirement:** The comment must prove soundness using *only* text from the
  stable [Rust Reference](https://doc.rust-lang.org/reference/) or [standard
  library documentation](https://doc.rust-lang.org/std/).
- **Citation:** You must cite and quote the relevant text from the
  documentation. Citations must cite a specific version of the documentation
  (e.g. https://doc.rust-lang.org/1.91.0/reference/ or
  https://doc.rust-lang.org/1.91.0/std/).
- **Prohibition:** Do not rely on "common sense" or behavior not guaranteed by
  the docs.
  ```rust
  // BAD: Missing justification for "obvious" properties.
  // SAFETY: `ptr` and `field` are from the same object.
  let offset = unsafe { field.cast::<u8>().offset_from(ptr.cast::<u8>()) };

  // GOOD: Explicitly justifies every requirement, even trivial ones.
  // SAFETY:
  // - `ptr` and `field` are derived from the same allocated object [1].
  // - The distance between them is trivially a multiple of `u8`'s size (1) [2],
  //   satisfying `offset_from`'s alignment requirement [1].
  //
  // [1] Per https://doc.rust-lang.org/1.91.0/std/primitive.pointer.html#method.offset_from:
  //
  //   Both pointers must be derived from the same allocated object, and the
  //   distance between them must be a multiple of the element size.
  //
  // [2] https://doc.rust-lang.org/1.91.0/reference/type-layout.html#primitive-data-layout
  let offset = unsafe { field.cast::<u8>().offset_from(ptr.cast::<u8>()) };
  ```
