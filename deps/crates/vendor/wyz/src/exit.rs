/*! `exit!` macro

The `exit!` macro simplifies exiting with an error code, and optionally printing
an error message prior to exit.

# Examples

This example exits with status `1`.

```rust,should_panic
wyz::exit!();
```

This example exits with status `2`.

```rust,should_panic
wyz::exit!(2);
```

This example exits with status `3`, and uses `eprintln!` to print an error
message before exiting. Note that if `stderr` has been closed, this will crash
the program with a panic due to `SIGPIPE`, and *not* call `process::exit()`.

```rust,should_panic
wyz::exit!(3, "Error status: {}", "testing");
```
!*/

#![cfg(feature = "std")]

/// `exit!` macro
#[macro_export]
macro_rules! exit {
	() => {
		$crate::exit!(1);
	};

	( $num:expr $(,)? ) => {
		::std::process::exit($num);
	};

	( $num:expr, $fmt:expr $( , $arg:expr )* $(,)? ) => {{
		eprintln!($fmt $( , $arg )*);
		$crate::exit!($num);
	}};
}
