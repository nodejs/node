// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![allow(unused)]

use std::{borrow::Cow, marker::PhantomData};
use zerofrom::ZeroFrom;
use zerovec::{maps::ZeroMapKV, ule::AsULE, VarZeroVec, ZeroMap, ZeroVec};

#[derive(ZeroFrom, Copy, Clone)]
struct IntExample {
    x: u32,
}

#[derive(ZeroFrom, Copy, Clone)]
struct GenericsExample<T> {
    x: u32,
    y: T,
}

#[derive(ZeroFrom, Copy, Clone)]
struct GenericsExampleWithDefault<T, U = usize> {
    x: T,
    y: U,
}

#[derive(ZeroFrom)]
struct CowExample<'a> {
    x: u8,
    y: &'a str,
    z: Cow<'a, str>,
    w: Cow<'a, [u8]>,
}

#[derive(ZeroFrom)]
struct ZeroVecExample<'a> {
    var: VarZeroVec<'a, str>,
    vec: ZeroVec<'a, u16>,
}

#[derive(ZeroFrom)]
struct ZeroVecExampleWithGenerics<'a, T: AsULE> {
    gen: ZeroVec<'a, T>,
    vec: ZeroVec<'a, u16>,
    bare: T,
}

#[derive(ZeroFrom)]
struct HasTuples<'data> {
    pub bar: (&'data str, &'data str),
}

fn assert_zf_tuples<'b>(x: &'b HasTuples) -> HasTuples<'b> {
    HasTuples::zero_from(x)
}
fn assert_zf_generics<'a, 'b>(
    x: &'b ZeroVecExampleWithGenerics<'a, u8>,
) -> ZeroVecExampleWithGenerics<'b, u8> {
    ZeroVecExampleWithGenerics::<'b, u8>::zero_from(x)
}

#[derive(ZeroFrom)]
struct ZeroMapGenericExample<'a, T: for<'b> ZeroMapKV<'b> + ?Sized> {
    map: ZeroMap<'a, str, T>,
}

fn assert_zf_map<'b>(x: &'b ZeroMapGenericExample<str>) -> ZeroMapGenericExample<'b, str> {
    ZeroMapGenericExample::zero_from(x)
}

#[derive(Clone, ZeroFrom)]
struct CloningZF1 {
    #[zerofrom(clone)] // Vec is not ZeroFrom, so it needs to be cloned
    vec: Vec<u8>,
}

#[derive(Clone, ZeroFrom)]
struct CloningZF2<'data> {
    #[zerofrom(clone)] // Cow is ZeroFrom, but we force a clone
    cow: Cow<'data, str>,
}

#[derive(ZeroFrom)]
enum CloningZF3<'data> {
    Cow(#[zerofrom(clone)] Cow<'data, str>),
}

#[derive(ZeroFrom)]
#[zerofrom(may_borrow(T, Q))] // instead of treating T as a copy type, we want to allow zerofromming T too
struct GenericsThatAreAlsoZf<T, Q: ?Sized, Ignored: ?Sized> {
    x: T,
    y: Option<T>,
    ignored: PhantomData<Ignored>,
    q: Q,
}

fn assert_zf_generics_may_borrow<'a, 'b>(
    x: &'b GenericsThatAreAlsoZf<&'a str, usize, str>,
) -> GenericsThatAreAlsoZf<&'b str, usize, str> {
    GenericsThatAreAlsoZf::<&'b str, usize, str>::zero_from(x)
}

#[derive(ZeroFrom)]
struct UsesGenericsThatAreAlsoZf<'a> {
    x: GenericsThatAreAlsoZf<&'a str, usize, str>,
}

// Ensure it works with invariant types too
#[derive(ZeroFrom)]
struct UsesGenericsThatAreAlsoZfWithMap<'a> {
    x: GenericsThatAreAlsoZf<ZeroMap<'a, str, str>, usize, str>,
}

fn main() {}
