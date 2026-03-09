// Copyright 2019 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

extern crate zerocopy_renamed;

#[path = "../include.rs"]
mod util;

use core::marker::PhantomData;

use zerocopy_renamed::{FromBytes, FromZeros, IntoBytes, TryFromBytes, Unaligned};

use self::util::util::NotZerocopy;

fn main() {}

// Test generic transparent structs

#[derive(IntoBytes, FromBytes, Unaligned)]
#[zerocopy(crate = "zerocopy_renamed")]
#[repr(transparent)]
struct TransparentStruct<T> {
    inner: T,
    _phantom: PhantomData<()>,
}

// It should be legal to derive these traits on a transparent struct, but it
// must also ensure the traits are only implemented when the inner type
// implements them.
util_assert_impl_all!(TransparentStruct<NotZerocopy>: TryFromBytes);
//~[msrv, stable, nightly]^ ERROR: the trait bound `NotZerocopy: zerocopy_renamed::TryFromBytes` is not satisfied

util_assert_impl_all!(TransparentStruct<NotZerocopy>: FromZeros);
//~[msrv, stable, nightly]^ ERROR: the trait bound `NotZerocopy: FromZeros` is not satisfied

util_assert_impl_all!(TransparentStruct<NotZerocopy>: FromBytes);
//~[msrv, stable, nightly]^ ERROR: the trait bound `NotZerocopy: zerocopy_renamed::FromBytes` is not satisfied

util_assert_impl_all!(TransparentStruct<NotZerocopy>: IntoBytes);
//~[msrv, stable, nightly]^ ERROR: the trait bound `NotZerocopy: zerocopy_renamed::IntoBytes` is not satisfied

util_assert_impl_all!(TransparentStruct<NotZerocopy>: Unaligned);
//~[msrv, stable, nightly]^ ERROR: the trait bound `NotZerocopy: zerocopy_renamed::Unaligned` is not satisfied
