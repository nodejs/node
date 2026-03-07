<div class="title-block" style="text-align: center;" align="center">

# `tap` <!-- omit in toc -->

## Suffix-Position Pipeline Behavior <!-- omit in toc -->

[![Crate][crate_img]][crate]
[![Documentation][docs_img]][docs]
[![License][license_img]][license_file]

[![Crate Downloads][downloads_img]][crate]
[![Crate Size][loc_img]][loc]

</div>

This crate provides extension methods on all types that allow transparent,
temporary, inspection/mutation (tapping), transformation (piping), or type
conversion. These methods make it convenient for you to insert debugging or
modification points into an expression without requiring you to change any other
portions of your code.

## Example Use

### Tapping

You can tap inside a method-chain expression for logging without requiring a
rebind. For instance, you may write a complex expression without any
intermediate debugging steps, and only later decide that you want them.
Ordinarily, this transform would look like this:

```rust
extern crate reqwest;
extern crate tracing;

// old
let body = reqwest::blocking::get("https://example.com")?
  .text()?;
tracing::debug!("Response contents: {}", body);

// new, with debugging
let resp = reqwest::blocking::get("https://example.com")?;
tracing::debug!("Response status: {}", resp.status());
let body = resp.text()?;
tracing::debug!("Response contents: {}", body);
```

while with tapping, you can plug the logging statement directly into the overall
expression, without making any other changes:

```rust
extern crate reqwest;
extern crate tracing;

let body = reqwest::blocking::get("https://example.com")?
  // The only change is the insertion of this line
  .tap(|resp| tracing::debug!("Response status: {}", resp.status()))
  .text()?;
tracing::debug!("Response contents: {}", body);
```

### Mutable Tapping

Some APIs are written to require mutable borrows, rather than value-to-value
transformations, which can require temporary rebinding in order to create
mutability in an otherwise-immutable context. For example, collecting data into
a vector, sorting the vector, and then freezing it, might look like this:

```rust
let mut collection = stream().collect::<Vec<_>>();
collection.sort();
// potential error site: inserting other mutations here
let collection = collection; // now immutable
```

But with a mutable tap, you can avoid the duplicate binding *and* guard against
future errors due to the presence of a mutable binding:

```rust
let collection = stream.collect::<Vec<_>>()
  .tap_mut(|v| v.sort());
```

The `.tap_mut()` and related methods provide a mutable borrow to their argument,
and allow the final binding site to choose their own level of mutability without
exposing the intermediate permission.

### Piping

In addition to transparent inspection or modification points, you may also wish
to use suffix calls for subsequent operations. For example, the standard library
offers the free function `fs::read` to convert `Path`-like objects into
`Vec<u8>` of their filesystem contents. Ordinarily, free functions require use
as:

```rust
use std::fs;

let mut path = get_base_path();
path.push("logs");
path.push(&format!("{}.log", today()));
let contents = fs::read(path)?;
```

whereäs use of tapping (for path modification) and piping (for `fs::read`) could
be expressed like this:

```rust
use std::fs;

let contents = get_base_path()
  .tap_mut(|p| p.push("logs"))
  .tap_mut(|p| p.push(&format!("{}.log", today())))
  .pipe(fs::read)?;
```

As a clearer example, consider the syntax required to apply multiple free
funtions without `let`-bindings looks like this:

```rust
let val = last(
  third(
    second(
      first(original_value),
      another_arg,
    )
  ),
  another_arg,
);
```

which requires reading the expression in alternating, inside-out, order, to
understand the full sequence of evaluation. With suffix calls, even free
functions can be written in a point-free style that maintains a clear temporal
and syntactic order:

```rust
let val = original_value
  .pipe(first)
  .pipe(|v| second(v, another_arg))
  .pipe(third)
  .pipe(|v| last(v, another_arg));
```

As piping is an ordinary method, not a syntax transformation, it still requires
that you write function-call expressions when using a function with multiple
arguments in the pipeline.

### Conversion

The `conv` module is the simplest: it provides two traits, `Conv` and `TryConv`,
which are sibling traits to `Into<T>` and `TryInto<T>`. Their methods,
`Conv::conv::<T>` and `TryConv::try_conv::<T>`, call the corresponding
trait implementation, and allow you to use `.into()`/`.try_into()` in
non-terminal method calls of an expression.

```rust
let bytes = "hello".into().into_bytes();
```

does not compile, because Rust cannot decide the type of `"hello".into()`.
Instead of rewriting the expression to use an intermediate `let` binding, you
can write it as

```rust
let bytes = "hello".conv::<String>().into_bytes();
```

## Full Functionality

The `Tap` and `Pipe` traits both provide a large number of methods, which use
different parts of the Rust language’s facilities for well-typed value access.
Rather than repeat the API documentation here, you should view the module items
in the [documentation][docs].

As a summary, these traits provide methods that, upon receipt of a value,

- apply no transformation
- apply an `AsRef` or `AsMut` implementation
- apply a `Borrow` or `BorrowMut` implementation
- apply the `Deref` or `DerefMut` implementation

before executing their effect argument.

In addition, each `Tap` method `.tap_x` has a sibling method `.tap_x_dbg` that
performs the same work, but only in debug builds; in release builds, the method
call is stripped. This allows you to leave debugging taps in your source code,
without affecting your project’s performance in true usage.

Lastly, the `tap` module also has traits `TapOptional` and `TapFallible` which
run taps on the variants of `Option` and `Result` enums, respectively, and do
nothing when the variant does not match the method name. `TapOptional::tap_some`
has no effect when called on a `None`, etc.

<!-- Badges -->
[crate]: https://crates.io/crates/tap "Crate Link"
[crate_img]: https://img.shields.io/crates/v/tap.svg?logo=rust "Crate Page"
[docs]: https://docs.rs/tap "Documentation"
[docs_img]: https://docs.rs/tap/badge.svg "Documentation Display"
[downloads_img]: https://img.shields.io/crates/dv/tap.svg?logo=rust "Crate Downloads"
[license_file]: https://github.com/myrrlyn/tap/blob/master/LICENSE.txt "License File"
[license_img]: https://img.shields.io/crates/l/tap.svg "License Display"
[loc]: https://github.com/myrrlyn/tap "Repository"
[loc_img]: https://tokei.rs/b1/github/myrrlyn/tap?category=code "Repository Size"
