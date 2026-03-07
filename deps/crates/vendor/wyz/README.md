<div align="center">

# `wyz` <!-- omit in toc -->

[![Crate][crate_img]][crate]
[![Documentation][docs_img]][docs]
[![License][license_img]][license_file]

[![Continuous Integration][travis_img]][travis]
[![Code Coverage][codecov_img]][codecov]
[![Crate Downloads][downloads_img]][crate]
[![Crate Size][loc_img]][loc]

</div>

I have developed a collection of utility and convenience Rust modules that are
useful to me, and may be useful to you also.

This crate is a collection of largely-independent small modules. I do not
currently offer features to disable modules independently of each other, but
their compilation cost is small enough to essentially not matter.

## Modules <!-- omit in toc -->

1. [`conv`](#conv)
1. [`exit`](#exit)
1. [`fmt`](#fmt)
1. [`pipe`](#pipe)
1. [`tap`](#tap)

## `conv`

This module provides a single trait, of the same name, with a single generic
method, also of the same name. This trait is a sibling to `Into`, but rather
than placing its type parameter in the trait (`Into::<T>::into`), `Conv` places
it in the method: `Conv::conv::<T>`.

By placing the type parameter in the method name, `.conv` can be called in
suffix position in expressions where the result type cannot be inferred and must
be explicitly stated.

```rust
use wyz::conv::Conv;

let digits = 0xabcd.conv::<String>().len();
```

This is a trivial example, but writing a code context where `.conv` makes sense
takes a lot more context than a `README` wants.

## `exit`

This is a macro that calls `std::process::exit`. It can return a status code,
and also print a message to `stderr`.

```rust
use wyz::exit::exit;

exit!();
exit!(2);
exit!(3, "This is a {} message", "failure");
```

The default call is `std::process::exit(1)`; a call may provide its own exit
code and, in addition, a set of arguments to pass directly to `eprintln!`. The
error message is not guaranteed to be emitted, as `stderr` may be closed at time
of `exit!`.

## `fmt`

Rust uses the `Debug` trait for automatic printing events in several parts of
the standard library. This module provides wrapper types which forward their
`Debug` implementation to a specified other formatting trait. It also implements
extension methods on all types that have format trait implementations to wrap
them in the corresponding shim type.

```rust
use wyz::fmt::FmtForward as _;

let val = 6;
let addr = &val as *const i32;
println!("{:?}", addr.fmt_pointer());
```

This snippet uses the `Debug` format template, but will print the `Pointer`
implementation of `*const i32`.

This is useful for fitting your values into an error-handling framework that
only uses `Debug`, such as the `fn main() -> Result` program layout.

## `pipe`

Rust does not permit universal suffix-position function call syntax. That is,
you can *always* call a function with `Scope::name(arguments…)`, but only *some*
functions can be called as `first_arg.name(other_args…)`. Working in “data
pipelines” – flows where the return value of one function is passed directly as
the first argument to the next – is common enough in our field that it has a
name in languages that support it: *method chaining*. A *method* is a function
that the language considers to be treated specially in regards to only its
first argument, and permits changing the abstract token series
`function arg1 args…` into the series `arg1 function args…`.

Rust restricts that order transformation to only functions defined in scope for
some type (either `impl Type` or `impl Trait for Type` blocks) and that take a
first argument named `self`.

Other languages permit calling *any* function, regardless of its definition site
in source code, in this manner, as long as the first argument is of the correct
type for the first parameter of the function.

In languages like F♯ and Elixir, this uses the call operator `|>` rather than
the C++ family’s `.` caller. This operator is pronounced `pipe`.

Rust does not have a pipe operator. The dot-caller is restricted to only the
implementation blocks listed above, and this is not likely to change because it
also performs limited type transformation operations in order to find a name
that fits.

This module provides a `Pipe` trait whose method, `pipe`, passes its `self`
first argument as the argument to its second-order function:

```rust
use wyz::pipe::Pipe;

let final = 5
  .pipe(|a| a + 10)
  .pipe(|a| a * 2);

assert_eq!(final, 30);
```

Without language-level syntax support, piping into closures always requires
restating the argument, and functions cannot curry the argument they receive
from `pipe` and arguments from the environment in the manner that dot-called
methods can.

```rust
fn fma(a: i32, b: i32, c: i32) -> i32 {
  (a * b) + c
}
5.pipe(|a| fma(a, 2, 3));

let fma_2_3 = |a| fma(a, 2, 3);
5.pipe(fma_2_3);
```

These are the only ways to express `5 |> fma(2, 3)`.

Sorry.

Bug the language team.

## `tap`

Tapping is a cousin operation to piping, except that rather than pass the
receiver by *value* into some function, and return the result of that function,
it passes a *borrow* of a value into a function, and then returns the original
value.

It is useful for inserting an operation into an expression without changing the
overall state (type or value) of the expression.

```rust
use wyz::tap::Tap;

let result = complex_value()
  .tap(|v| log::info!("First stage: {}", v))
  .transform(other, args)
  .tap(|v| log::info!("Second stage: {}", v));
```

The tap calls have no effect on the expression into which they are placed,
except to induce side effects somewhere else in the system. Commenting out the
two `.tap` calls does not change anything about `complex_value()`, `.transform`,
or `result`; it only removes the log statements.

This enables easily inserting or removing inspection points in an expression
without otherwise altering it.

[codecov]: https://codecov.io/gh/myrrlyn/wyz "Code Coverage"
[codecov_img]: https://img.shields.io/codecov/c/github/myrrlyn/wyz.svg?logo=codecov "Code Coverage Display"
[crate]: https://crates.io/crates/wyz "Crate Link"
[crate_img]: https://img.shields.io/crates/v/wyz.svg?logo=rust "Crate Page"
[docs]: https://docs.rs/wyz "Documentation"
[docs_img]: https://docs.rs/wyz/badge.svg "Documentation Display"
[downloads_img]: https://img.shields.io/crates/dv/wyz.svg?logo=rust "Crate Downloads"
[license_file]: https://github.com/myrrlyn/wyz/blob/master/LICENSE.txt "License File"
[license_img]: https://img.shields.io/crates/l/wyz.svg "License Display"
[loc]: https://github.com/myrrlyn/wyz "Repository"
[loc_img]: https://tokei.rs/b1/github/myrrlyn/wyz?category=code "Repository Size"
[travis]: https://travis-ci.org/myrrlyn/wyz "Travis CI"
[travis_img]: https://img.shields.io/travis/myrrlyn/wyz.svg?logo=travis "Travis CI Display"
