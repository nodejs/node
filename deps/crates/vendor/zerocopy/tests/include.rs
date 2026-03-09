// Copyright 2026 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

extern crate zerocopy;

#[allow(unused)]
#[cfg(feature = "derive")]
mod util {
    /// A type that doesn't implement any zerocopy traits.
    pub struct NotZerocopy<T = ()>(pub T);

    /// A `u16` with alignment 2.
    ///
    /// Though `u16` has alignment 2 on some platforms, it's not guaranteed. By
    /// contrast, `util::AU16` is guaranteed to have alignment 2.
    #[derive(
        zerocopy::KnownLayout,
        zerocopy::Immutable,
        zerocopy::FromBytes,
        zerocopy::IntoBytes,
        Copy,
        Clone,
    )]
    #[repr(C, align(2))]
    pub struct AU16(pub u16);

    // Since we can't import these by path (ie, `util::assert_impl_all!`), use a
    // name prefix to ensure our derive-emitted code isn't accidentally relying
    // on `assert_impl_all!` being in scope.
    #[macro_export]
    macro_rules! util_assert_impl_all {
        ($type:ty: $($trait:path),+ $(,)?) => {
            const _: fn() = || {
                use ::core::prelude::v1::*;
                ::static_assertions::assert_impl_all!($type: $($trait),+);
            };
        };
    }

    // Since we can't import these by path (ie, `util::assert_not_impl_any!`),
    // use a name prefix to ensure our derive-emitted code isn't accidentally
    // relying on `assert_not_impl_any!` being in scope.
    #[macro_export]
    macro_rules! util_assert_not_impl_any {
        ($x:ty: $($t:path),+ $(,)?) => {
            const _: fn() = || {
                use ::core::prelude::v1::*;
                ::static_assertions::assert_not_impl_any!($x: $($t),+);
            };
        };
    }

    #[macro_export]
    macro_rules! test_trivial_is_bit_valid {
        ($x:ty => $name:ident) => {
            #[test]
            fn $name() {
                util::test_trivial_is_bit_valid::<$x>();
            }
        };
    }
}
