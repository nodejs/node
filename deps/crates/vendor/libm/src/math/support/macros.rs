/// `libm` cannot have dependencies, so this is vendored directly from the `cfg-if` crate
/// (with some comments stripped for compactness).
macro_rules! cfg_if {
    // match if/else chains with a final `else`
    ($(
        if #[cfg($meta:meta)] { $($tokens:tt)* }
    ) else * else {
        $($tokens2:tt)*
    }) => {
        cfg_if! { @__items () ; $( ( ($meta) ($($tokens)*) ), )* ( () ($($tokens2)*) ), }
    };

    // match if/else chains lacking a final `else`
    (
        if #[cfg($i_met:meta)] { $($i_tokens:tt)* }
        $( else if #[cfg($e_met:meta)] { $($e_tokens:tt)* } )*
    ) => {
        cfg_if! {
            @__items
            () ;
            ( ($i_met) ($($i_tokens)*) ),
            $( ( ($e_met) ($($e_tokens)*) ), )*
            ( () () ),
        }
    };

    // Internal and recursive macro to emit all the items
    //
    // Collects all the negated cfgs in a list at the beginning and after the
    // semicolon is all the remaining items
    (@__items ($($not:meta,)*) ; ) => {};
    (@__items ($($not:meta,)*) ; ( ($($m:meta),*) ($($tokens:tt)*) ), $($rest:tt)*) => {
        #[cfg(all($($m,)* not(any($($not),*))))] cfg_if! { @__identity $($tokens)* }
        cfg_if! { @__items ($($not,)* $($m,)*) ; $($rest)* }
    };

    // Internal macro to make __apply work out right for different match types,
    // because of how macros matching/expand stuff.
    (@__identity $($tokens:tt)*) => { $($tokens)* };
}

/// Choose between using an arch-specific implementation and the function body. Returns directly
/// if the arch implementation is used, otherwise continue with the rest of the function.
///
/// Specify a `use_arch` meta field if an architecture-specific implementation is provided.
/// These live in the `math::arch::some_target_arch` module.
///
/// Specify a `use_arch_required` meta field if something architecture-specific must be used
/// regardless of feature configuration (`force-soft-floats`).
///
/// The passed meta options do not need to account for the `arch` target feature.
macro_rules! select_implementation {
    (
        name: $fn_name:ident,
        // Configuration meta for when to use arch-specific implementation that requires hard
        // float ops
        $( use_arch: $use_arch:meta, )?
        // Configuration meta for when to use the arch module regardless of whether softfloats
        // have been requested.
        $( use_arch_required: $use_arch_required:meta, )?
        args: $($arg:ident),+ ,
    ) => {
        // FIXME: these use paths that are a pretty fragile (`super`). We should figure out
        // something better w.r.t. how this is vendored into compiler-builtins.

        // However, we do need a few things from `arch` that are used even with soft floats.
        select_implementation! {
            @cfg $($use_arch_required)?;
            if true {
                return  super::arch::$fn_name( $($arg),+ );
            }
        }

        // By default, never use arch-specific implementations if we have force-soft-floats
        #[cfg(arch_enabled)]
        select_implementation! {
            @cfg $($use_arch)?;
            // Wrap in `if true` to avoid unused warnings
            if true {
                return  super::arch::$fn_name( $($arg),+ );
            }
        }
    };

    // Coalesce helper to construct an expression only if a config is provided
    (@cfg ; $ex:expr) => { };
    (@cfg $provided:meta; $ex:expr) => { #[cfg($provided)] $ex };
}

/// Construct a 16-bit float from hex float representation (C-style), guaranteed to
/// evaluate at compile time.
#[cfg(f16_enabled)]
#[cfg_attr(feature = "unstable-public-internals", macro_export)]
#[allow(unused_macros)]
macro_rules! hf16 {
    ($s:literal) => {{
        const X: f16 = $crate::support::hf16($s);
        X
    }};
}

/// Construct a 32-bit float from hex float representation (C-style), guaranteed to
/// evaluate at compile time.
#[allow(unused_macros)]
#[cfg_attr(feature = "unstable-public-internals", macro_export)]
macro_rules! hf32 {
    ($s:literal) => {{
        const X: f32 = $crate::support::hf32($s);
        X
    }};
}

/// Construct a 64-bit float from hex float representation (C-style), guaranteed to
/// evaluate at compile time.
#[allow(unused_macros)]
#[cfg_attr(feature = "unstable-public-internals", macro_export)]
macro_rules! hf64 {
    ($s:literal) => {{
        const X: f64 = $crate::support::hf64($s);
        X
    }};
}

/// Construct a 128-bit float from hex float representation (C-style), guaranteed to
/// evaluate at compile time.
#[cfg(f128_enabled)]
#[allow(unused_macros)]
#[cfg_attr(feature = "unstable-public-internals", macro_export)]
macro_rules! hf128 {
    ($s:literal) => {{
        const X: f128 = $crate::support::hf128($s);
        X
    }};
}

/// Assert `F::biteq` with better messages.
#[cfg(test)]
macro_rules! assert_biteq {
    ($left:expr, $right:expr, $($tt:tt)*) => {{
        use $crate::support::Int;
        let l = $left;
        let r = $right;
        let bits = Int::leading_zeros(l.to_bits() - l.to_bits()); // hack to get the width from the value
        assert!(
            l.biteq(r),
            "{}\nl: {l:?} ({lb:#0width$x})\nr: {r:?} ({rb:#0width$x})",
            format_args!($($tt)*),
            lb = l.to_bits(),
            rb = r.to_bits(),
            width = ((bits / 4) + 2) as usize,

        );
    }};
    ($left:expr, $right:expr $(,)?) => {
        assert_biteq!($left, $right, "")
    };
}
