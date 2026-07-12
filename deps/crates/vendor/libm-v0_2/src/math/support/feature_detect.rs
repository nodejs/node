//! Helpers for runtime target feature detection that are shared across architectures.

// `AtomicU32` is preferred for a consistent size across targets.
#[cfg(all(target_has_atomic = "ptr", not(target_has_atomic = "32")))]
compile_error!("currently all targets that support `AtomicPtr` also support `AtomicU32`");

use core::sync::atomic::{AtomicU32, Ordering};

/// Given a list of identifiers, assign each one a unique sequential single-bit mask.
#[allow(unused_macros)]
macro_rules! unique_masks {
    ($ty:ty, $($name:ident,)+) => {
        #[cfg(test)]
        pub const ALL: &[$ty] = &[$($name),+];
        #[cfg(test)]
        pub const NAMES: &[&str] = &[$(stringify!($name)),+];

        unique_masks!(@one; $ty; 0; $($name,)+);
    };
    // Matcher for a single value
    (@one; $_ty:ty; $_idx:expr;) => {};
    (@one; $ty:ty; $shift:expr; $name:ident, $($tail:tt)*) => {
        pub const $name: $ty = 1 << $shift;
        // Ensure the top bit is not used since it stores initialized state.
        const _: () = assert!($name != (1 << (<$ty>::BITS - 1)));
        // Increment the shift and invoke the next
        unique_masks!(@one; $ty; $shift + 1; $($tail)*);
    };
}

/// Call `init` once to choose an implementation, then use it for the rest of the program.
///
/// - `sig` is the function type.
/// - `init` is an expression called at startup that chooses an implementation and returns a
///   function pointer.
/// - `call` is an expression to call a function returned by `init`, encapsulating any safety
///   preconditions.
///
/// The type `Func` is available in `init` and `call`.
///
/// This is effectively our version of an ifunc without linker support. Note that `init` may be
/// called more than once until one completes.
#[allow(unused_macros)] // only used on some architectures
macro_rules! select_once {
    (
        sig: fn($($arg:ident: $ArgTy:ty),*) -> $RetTy:ty,
        init: $init:expr,
        call: $call:expr,
    ) => {{
        use core::mem;
        use core::sync::atomic::{AtomicPtr, Ordering};

        type Func = unsafe fn($($arg: $ArgTy),*) -> $RetTy;

        /// Stores a pointer that is immediately jumped to. By default it is an init function
        /// that sets FUNC to something else.
        static FUNC: AtomicPtr<()> = AtomicPtr::new((initializer as Func) as *mut ());

        /// Run once to set the function that will be used for all subsequent calls.
        fn initializer($($arg: $ArgTy),*) -> $RetTy {
            // Select an implementation, ensuring a 'static lifetime.
            let fn_ptr: Func = $init();
            FUNC.store(fn_ptr as *mut (), Ordering::Relaxed);

            // Forward the call to the selected function.
            $call(fn_ptr)
        }

        let raw: *mut () = FUNC.load(Ordering::Relaxed);

        // SAFETY: will only ever be `initializer` or another function pointer that has the
        // 'static lifetime.
        let fn_ptr: Func = unsafe { mem::transmute::<*mut (), Func>(raw) };

        $call(fn_ptr)
    }}
}

#[allow(unused_imports)]
pub(crate) use {select_once, unique_masks};

use crate::support::cold_path;

/// Helper for working with bit flags, based on `bitflags`.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Flags(u32);

#[allow(dead_code)] // only used on some architectures
impl Flags {
    /// No bits set.
    pub const fn empty() -> Self {
        Self(0)
    }

    /// Create with bits already set.
    pub const fn from_bits(val: u32) -> Self {
        Self(val)
    }

    /// Get the integer representation.
    pub fn bits(&self) -> u32 {
        self.0
    }

    /// Set any bits in `mask`.
    pub fn insert(&mut self, mask: u32) {
        self.0 |= mask;
    }

    /// Check whether the mask is set.
    pub fn contains(&self, mask: u32) -> bool {
        self.0 & mask == mask
    }

    /// Check whether the nth bit is set.
    pub fn test_nth(&self, bit: u32) -> bool {
        debug_assert!(bit < u32::BITS, "bit index out-of-bounds");
        self.0 & (1 << bit) != 0
    }
}

/// Load flags from an atomic value. If the flags have not yet been initialized, call `init`
/// to do so.
///
/// Note that `init` may run more than once.
#[allow(dead_code)] // only used on some architectures
pub fn get_or_init_flags_cache(cache: &AtomicU32, init: impl FnOnce() -> Flags) -> Flags {
    // The top bit is used to indicate that the values have already been set once.
    const INITIALIZED: u32 = 1 << 31;

    // Relaxed ops are sufficient since the result should always be the same.
    let mut flags = Flags::from_bits(cache.load(Ordering::Relaxed));

    if !flags.contains(INITIALIZED) {
        // Without this, `init` is inlined and the bit check gets wrapped in `init`'s lengthy
        // prologue/epilogue. Cold pathing gives a preferable load->test->?jmp->ret.
        cold_path();

        flags = init();
        debug_assert!(
            !flags.contains(INITIALIZED),
            "initialized bit shouldn't be set"
        );
        flags.insert(INITIALIZED);
        cache.store(flags.bits(), Ordering::Relaxed);
    }

    flags
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn unique_masks() {
        unique_masks! {
            u32,
            V0,
            V1,
            V2,
        }
        assert_eq!(V0, 1u32 << 0);
        assert_eq!(V1, 1u32 << 1);
        assert_eq!(V2, 1u32 << 2);
        assert_eq!(ALL, [V0, V1, V2]);
        assert_eq!(NAMES, ["V0", "V1", "V2"]);
    }

    #[test]
    fn flag_cache_is_used() {
        // Sanity check that flags are only ever set once
        static CACHE: AtomicU32 = AtomicU32::new(0);

        let mut f1 = Flags::from_bits(0x1);
        let f2 = Flags::from_bits(0x2);

        let r1 = get_or_init_flags_cache(&CACHE, || f1);
        let r2 = get_or_init_flags_cache(&CACHE, || f2);

        f1.insert(1 << 31); // init bit

        assert_eq!(r1, f1);
        assert_eq!(r2, f1);
    }

    #[test]
    fn select_cache_is_used() {
        // Sanity check that cache is used
        static CALLED: AtomicU32 = AtomicU32::new(0);

        fn inner() {
            fn nop() {}

            select_once! {
                sig: fn() -> (),
                init: || {
                    CALLED.fetch_add(1, Ordering::Relaxed);
                    nop
                },
                call: |fn_ptr: Func| unsafe { fn_ptr() },
            }
        }

        // `init` should only have been called once.
        inner();
        assert_eq!(CALLED.load(Ordering::Relaxed), 1);
        inner();
        assert_eq!(CALLED.load(Ordering::Relaxed), 1);
    }
}
