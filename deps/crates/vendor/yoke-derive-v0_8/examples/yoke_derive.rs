// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![allow(unused)]

use std::{borrow::Cow, marker::PhantomData};
use yoke::{Yoke, Yokeable};
use zerovec::{maps::ZeroMapKV, ule::AsULE, VarZeroVec, ZeroMap, ZeroVec};

#[derive(Yokeable)]
struct StringExample {
    x: String,
}

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
struct ManualStringExample {
    x: String,
}

#[derive(Yokeable, Copy, Clone)]
struct IntExample {
    x: u32,
}

#[derive(Yokeable, Copy, Clone)]
#[yoke(prove_covariance_manually)]
struct ManualIntExample {
    x: u32,
}

#[derive(Yokeable, Copy, Clone)]
struct GenericsExample<T> {
    x: u32,
    y: T,
}

#[derive(Yokeable, Copy, Clone)]
#[yoke(prove_covariance_manually)]
struct ManualGenericsExample<T> {
    x: u32,
    y: T,
}

#[derive(Yokeable, Copy, Clone)]
struct GenericsExampleWithDefault<T, U = usize> {
    x: T,
    y: U,
}

#[derive(Yokeable, Copy, Clone)]
#[yoke(prove_covariance_manually)]
struct ManualGenericsExampleWithDefault<T, U = usize> {
    x: T,
    y: U,
}

#[derive(Yokeable)]
struct CowExample<'a> {
    x: u8,
    y: &'a str,
    z: Cow<'a, str>,
    w: Cow<'a, [u8]>,
}

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
struct ManualCowExample<'a> {
    x: u8,
    y: &'a str,
    z: Cow<'a, str>,
    w: Cow<'a, [u8]>,
}

#[derive(Yokeable)]
struct ZeroVecExample<'not_a> {
    var: VarZeroVec<'not_a, str>,
    vec: ZeroVec<'not_a, u16>,
}

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
struct ManualZeroVecExample<'not_a> {
    var: VarZeroVec<'not_a, str>,
    vec: ZeroVec<'not_a, u16>,
}

#[derive(Yokeable)]
struct ZeroVecExampleWithGenerics<'a, T: AsULE> {
    gen: ZeroVec<'a, T>,
    vec: ZeroVec<'a, u16>,
    bare: T,
}

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
struct ManualZeroVecExampleWithGenerics<'a, T: AsULE> {
    gen: ZeroVec<'a, T>,
    vec: ZeroVec<'a, u16>,
    bare: T,
}

// Since ZeroMap has generic parameters, the Rust compiler cannot
// prove the covariance of the lifetimes. To use derive(Yokeable)
// with a type such as ZeroMap, you just add the attribute
// yoke(prove_covariance_manually)
#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
struct ManualZeroMapExample<'a> {
    map: ZeroMap<'a, str, u16>,
}

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
struct ManualZeroMapGenericExample<'a, T: for<'b> ZeroMapKV<'b> + ?Sized> {
    map: ZeroMap<'a, str, T>,
}

#[derive(Yokeable)]
struct MaybeSizedWrap<T, Q: ?Sized, U: ?Sized> {
    x: T,
    y: Option<T>,
    ignored: PhantomData<U>,
    q: Q,
}

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
struct ManualMaybeSizedWrap<T, Q: ?Sized, U: ?Sized> {
    x: T,
    y: Option<T>,
    ignored: PhantomData<U>,
    q: Q,
}

// TODO(#4119): Make this example compile
/*
#[derive(Yokeable)]
struct MaybeSizedWrapWithLifetime<'a, T, Q: ?Sized, U: ?Sized> {
    x: T,
    y: Option<T>,
    ignored: &'a U,
    q: Q,
}
*/

trait Trait {}
impl Trait for u32 {}

#[derive(Yokeable)]
struct WithTraitBounds<T: Trait> {
    x: T,
}

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
struct ManualWithTraitBounds<T: Trait> {
    x: T,
}

#[derive(Yokeable)]
struct WithTraitBoundsInWhere<T>
where
    T: Trait,
{
    x: T,
}

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
struct ManualWithTraitBoundsInWhere<T>
where
    T: Trait,
{
    x: T,
}

#[derive(Yokeable)]
struct BoundFn<'a>(&'a (), for<'b> fn(&'b ()));

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
struct ManualBoundFn<'a>(&'a (), for<'b> fn(&'b ()));

#[derive(Yokeable)]
struct Invariant<'a, T>(&'a mut T);

#[derive(Yokeable)]
struct InvariantStatic<'a> {
    field: Invariant<'a, &'static str>,
}

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
struct ManualInvariantStatic<'a> {
    field: Invariant<'a, &'static str>,
}

#[derive(Yokeable)]
struct RawLifetime<'r#mod> {
    field: &'r#mod str,
    other: for<'r#yoke> fn(&'r#yoke ()),
}

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
struct ManualRawLifetime<'r#mod> {
    field: &'r#mod str,
    other: for<'r#yoke> fn(&'r#yoke ()),
}

#[derive(Yokeable)]
struct MixedRawLifetime<'r#a> {
    field: &'r#a &'a str,
}

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
struct ManualMixedRawLifetime<'r#a> {
    field: &'r#a &'a str,
}

#[derive(Yokeable)]
struct YokeLifetime<'r#_yoke>(&'r#_yoke str, for<'yoke> fn(&'yoke ()));

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
struct ManualYokeLifetime<'r#_yoke>(&'r#_yoke str, for<'yoke> fn(&'yoke ()));

// TODO: make this compile
// #[derive(Yokeable)]
struct ConstGenerics<'a, const N: u32> {
    field: &'a str,
}

// #[derive(Yokeable)]
// #[yoke(prove_covariance_manually)]
struct ManualConstGenerics<'a, const N: u32> {
    field: &'a str,
}

#[derive(Yokeable)]
struct Variadic<'a> {
    field: fn(extern "C" fn(&'a (), u32, u32, ...)),
}

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
struct ManualVariadic<'a> {
    // `fn(..)` does not implement `Yokeable`, even if the type is in fact covariant.
    // Thus the extra `&'a`.
    field: &'a fn(extern "C" fn(&'a (), u32, u32, ...)),
}

#[derive(Yokeable)]
enum Enum {
    A,
    B,
}

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
enum ManualEnum {
    A,
    B,
}

#[derive(Yokeable)]
enum Empty {}

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
enum ManualEmpty {}

#[derive(Yokeable)]
enum Uninhabited<'a> {
    Variant(Empty, &'a ()),
}

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
enum ManualUninhabited<'a> {
    Variant(Empty, &'a ()),
}

// The random extra `r#`s are intentional
#[derive(Yokeable)]
enum AlmostEverything<'a, T, ULE: AsULE, r#W, /*const N: u32,*/ U = usize>
where
    W: Trait,
{
    X(String),
    X2 {
        x: u32,
    },
    Y(T),
    Gen {
        x: T,
        y: U,
    },
    CowExample {
        x: u8,
        y: &'a str,
        z: Cow<'a, str>,
        w: Cow<'a, [u8]>,
    },
    ZeroVec {
        var: VarZeroVec<'a, str>,
        vec: ZeroVec<'a, u16>,
    },
    ZeroVecGen {
        gen: r#ZeroVec<'a, ULE>,
        vec: r#ZeroVec<'a, u16>,
        bare: T,
    },
    Phantom(T, Option<T>, PhantomData<&'a ()>),
    TraitBounds(W),
    BoundFn(for<'yoke> fn(&'yoke ())),
    RawLifetime(for<'r#_yoke> fn(&'r#_yoke ())),
    MixedRawLifetime(&'r#a &'a str),
    Variadic(fn(extern "C" fn(&'a (), u32, u32, ...))),
}

#[derive(Yokeable)]
#[yoke(prove_covariance_manually)]
enum ManualAlmostEverything<
    'a,
    T,
    ULE: AsULE,
    r#W,
    Z: for<'b> ZeroMapKV<'b> + ?Sized,
    // const N: u32,
    U = usize,
> where
    W: Trait,
{
    X(String),
    X2 {
        x: u32,
    },
    Y(T),
    Gen {
        x: T,
        y: U,
    },
    CowExample {
        x: u8,
        y: &'a str,
        z: Cow<'a, str>,
        w: Cow<'a, [u8]>,
    },
    ZeroVec {
        var: VarZeroVec<'a, str>,
        vec: ZeroVec<'a, u16>,
    },
    ZeroVecGen {
        gen: r#ZeroVec<'a, ULE>,
        vec: r#ZeroVec<'a, u16>,
        bare: T,
    },
    Phantom(T, Option<T>, PhantomData<&'a ()>),
    TraitBounds(W),
    BoundFn(for<'yoke> fn(&'yoke ())),
    RawLifetime(for<'r#_yoke> fn(&'r#_yoke ())),
    MixedRawLifetime(&'r#a &'a str),
    Variadic(&'a fn(extern "C" fn(&'a (), u32, u32, ...))),
    Map(ZeroMap<'a, str, u16>, ZeroMap<'a, str, Z>),
}

struct AssertYokeable {
    string: Yoke<StringExample, Box<[u8]>>,
    int: Yoke<IntExample, Box<[u8]>>,
    gen1: Yoke<GenericsExample<u32>, Box<[u8]>>,
    gen2: Yoke<GenericsExample<String>, Box<[u8]>>,
    gen_default1: Yoke<GenericsExampleWithDefault<String>, Box<[u8]>>,
    gen_default2: Yoke<GenericsExampleWithDefault<String, u8>, Box<[u8]>>,
    cow: Yoke<CowExample<'static>, Box<[u8]>>,
    zv: Yoke<ZeroVecExample<'static>, Box<[u8]>>,
    zv_gen1: Yoke<ZeroVecExampleWithGenerics<'static, u8>, Box<[u8]>>,
    zv_gen2: Yoke<ZeroVecExampleWithGenerics<'static, char>, Box<[u8]>>,
    maybe_sized_wrap: Yoke<MaybeSizedWrap<usize, usize, str>, Box<[u8]>>,
    // TODO(#4119): Make this example compile
    // maybe_sized_wrap_with_lt: Yoke<MaybeSizedWrapWithLifetime<'static, usize, usize, str>, Box<[u8]>>,
    trait_bounds: Yoke<WithTraitBounds<u32>, Box<[u8]>>,
    trait_bounds_where: Yoke<WithTraitBoundsInWhere<u32>, Box<[u8]>>,
    bound_fn: Yoke<BoundFn<'static>, Box<[u8]>>,
    invariant_static: Yoke<InvariantStatic<'static>, Box<[u8]>>,
    raw: Yoke<RawLifetime<'static>, Box<[u8]>>,
    mixed_raw: Yoke<MixedRawLifetime<'static>, Box<[u8]>>,
    yoke_lt: Yoke<YokeLifetime<'static>, Box<[u8]>>,
    // const_gen: Yoke<ConstGenerics<'static>, Box<[u8]>>,
    variadic: Yoke<Variadic<'static>, Box<[u8]>>,
    simple_enum: Yoke<Enum, Box<[u8]>>,
    empty: Yoke<Empty, Box<[u8]>>,
    uninhabited: Yoke<Uninhabited<'static>, Box<[u8]>>,
    almost_everything: Yoke<AlmostEverything<'static, u8, u32, u32>, Box<[u8]>>,
}

struct AssertYokeableManual {
    string: Yoke<ManualStringExample, Box<[u8]>>,
    int: Yoke<ManualIntExample, Box<[u8]>>,
    gen1: Yoke<ManualGenericsExample<u32>, Box<[u8]>>,
    gen2: Yoke<ManualGenericsExample<String>, Box<[u8]>>,
    gen_default1: Yoke<ManualGenericsExampleWithDefault<String>, Box<[u8]>>,
    gen_default2: Yoke<ManualGenericsExampleWithDefault<String, u8>, Box<[u8]>>,
    cow: Yoke<ManualCowExample<'static>, Box<[u8]>>,
    zv: Yoke<ManualZeroVecExample<'static>, Box<[u8]>>,
    zv_gen1: Yoke<ManualZeroVecExampleWithGenerics<'static, u8>, Box<[u8]>>,
    zv_gen2: Yoke<ManualZeroVecExampleWithGenerics<'static, char>, Box<[u8]>>,
    map: Yoke<ManualZeroMapExample<'static>, Box<[u8]>>,
    map_gen1: Yoke<ManualZeroMapGenericExample<'static, u32>, Box<[u8]>>,
    map_gen2: Yoke<ManualZeroMapGenericExample<'static, str>, Box<[u8]>>,
    maybe_sized_wrap: Yoke<ManualMaybeSizedWrap<usize, usize, str>, Box<[u8]>>,
    trait_bounds: Yoke<ManualWithTraitBounds<u32>, Box<[u8]>>,
    trait_bounds_where: Yoke<ManualWithTraitBoundsInWhere<u32>, Box<[u8]>>,
    bound_fn: Yoke<ManualBoundFn<'static>, Box<[u8]>>,
    invariant_static: Yoke<ManualInvariantStatic<'static>, Box<[u8]>>,
    raw: Yoke<ManualRawLifetime<'static>, Box<[u8]>>,
    mixed_raw: Yoke<ManualMixedRawLifetime<'static>, Box<[u8]>>,
    yoke_lt: Yoke<ManualYokeLifetime<'static>, Box<[u8]>>,
    // const_gen: Yoke<ManualConstGenerics<'static>, Box<[u8]>>,
    variadic: Yoke<ManualVariadic<'static>, Box<[u8]>>,
    simple_enum: Yoke<ManualEnum, Box<[u8]>>,
    empty: Yoke<ManualEmpty, Box<[u8]>>,
    uninhabited: Yoke<ManualUninhabited<'static>, Box<[u8]>>,
    almost_everything: Yoke<ManualAlmostEverything<'static, u8, u32, u32, str>, Box<[u8]>>,
}

fn main() {}
