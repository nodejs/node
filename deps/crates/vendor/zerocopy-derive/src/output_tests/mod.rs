// Copyright 2024 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

use std::path::Path;

use dissimilar::Chunk;
use proc_macro2::TokenStream;

use crate::IntoTokenStream;

macro_rules! use_as_trait_name {
    ($($alias:ident => $derive:path),* $(,)?) => {
        $(use $derive as $alias;)*
    };
}

// This permits invocations of `test!` to be more ergonomic, passing the name of
// the trait under test rather than the name of the inner derive function.
use_as_trait_name!(
    KnownLayout => super::derive::known_layout::derive,
    Immutable => super::derive::derive_immutable,
    TryFromBytes => super::derive::try_from_bytes::derive_try_from_bytes,
    FromZeros => super::derive::from_bytes::derive_from_zeros,
    FromBytes => super::derive::from_bytes::derive_from_bytes,
    IntoBytes => super::derive::into_bytes::derive_into_bytes,
    Unaligned => super::derive::unaligned::derive_unaligned,
    ByteHash => super::derive::derive_hash,
    ByteEq => super::derive::derive_eq,
    SplitAt => super::derive::derive_split_at,
);

/// Test that the given derive input expands to the expected output.
///
/// Equality is tested by formatting both token streams using `prettyplease` and
/// performing string equality on the results. This has the effect of making the
/// tests less brittle and robust against meaningless formatting changes.
// Adapted from https://github.com/joshlf/synstructure/blob/400499aaf54840056ff56718beb7810540e6be59/src/macros.rs#L212-L317
macro_rules! test {
    ($name:ident { $($i:tt)* } expands to { $($o:tt)* }) => {
        {
            #[allow(dead_code)]
            fn ensure_compiles() {
                $($i)*
                $($o)*
            }

            test!($name { $($i)* } expands to { $($o)* } no_build);
        }
    };

    ($name:ident { $($i:tt)* } expands to { $($o:tt)* } no_build) => {
        {
            let ts: proc_macro2::TokenStream = quote::quote!( $($i)* );
            let ast = syn::parse2::<syn::DeriveInput>(ts).unwrap();
            let ctx = crate::Ctx::try_from_derive_input(ast).unwrap();
            let res = $name(&ctx, crate::util::Trait::$name);
            let expected_toks = quote::quote!( $($o)* );
            let expected = pretty_print(expected_toks);
            let actual = pretty_print(res.into_ts().into());
            assert_eq_or_diff(&expected, &actual);
        }
    };

    ($name:ident { $($i:tt)* } expands to $path:expr) => {
        test!($name { $($i)* } expands to $path; no_build);
    };

    ($name:ident { $($i:tt)* } expands to $path:expr; no_build) => {
        {
            let ts: proc_macro2::TokenStream = quote::quote!( $($i)* );
            let ast = syn::parse2::<syn::DeriveInput>(ts).unwrap();
            let ctx = crate::Ctx::try_from_derive_input(ast).unwrap();
            let res = $name(&ctx, crate::util::Trait::$name);
            let actual = pretty_print(res.into_ts().into());

            if std::env::var("ZEROCOPY_BLESS").is_ok() {
                let path = Path::new(env!("CARGO_MANIFEST_DIR"))
                    .join("src/output_tests")
                    .join($path);
                std::fs::write(&path, &actual).expect("failed to bless output");
            } else {
                let expected_str = include_str!($path);
                let expected_ts: proc_macro2::TokenStream = expected_str.parse().expect("failed to parse expected output");
                let expected = pretty_print(expected_ts);
                assert_eq_or_diff(&expected, &actual);
            }
        }
    };
}

fn pretty_print(ts: TokenStream) -> String {
    prettyplease::unparse(&syn::parse_file(&ts.to_string()).unwrap())
}

#[track_caller]
fn assert_eq_or_diff(expected: &str, actual: &str) {
    if expected != actual {
        let diff = dissimilar::diff(expected, actual)
            .into_iter()
            .flat_map(|chunk| {
                let (prefix, chunk) = match chunk {
                    Chunk::Equal(chunk) => (" ", chunk),
                    Chunk::Delete(chunk) => ("-", chunk),
                    Chunk::Insert(chunk) => ("+", chunk),
                };
                [prefix, chunk, "\n"]
            })
            .collect::<String>();

        panic!(
            "\
test failed:
got:
```
{}
```

diff (expected vs got):
```
{}
```\n",
            actual, diff
        );
    }
}

#[test]
fn test_known_layout_struct() {
    test! {
        KnownLayout {
            struct Foo;
        } expands to "expected/known_layout_struct.expected.rs"
    }
}

#[test]
fn test_known_layout_repr_c_struct() {
    test! {
        KnownLayout {
            #[repr(C, align(2))]
            struct Foo<T, U>(T, U);
        }
        expands to "expected/known_layout_repr_c_struct.expected.rs"
    }
}

#[test]
fn test_immutable() {
    test! {
        Immutable {
            struct Foo;
        } expands to "expected/immutable.expected.rs"
    }
}

#[test]
fn test_try_from_bytes() {
    test! {
        TryFromBytes {
            struct Foo;
        } expands to "expected/try_from_bytes.expected.rs"
    }
}

#[test]
fn test_from_zeros() {
    test! {
        FromZeros {
            struct Foo;
        } expands to "expected/from_zeros.expected.rs"
    }
}

#[test]
fn test_from_bytes_struct() {
    test! {
        FromBytes {
            struct Foo;
        } expands to "expected/from_bytes_struct.expected.rs"
    }
}

#[test]
fn test_from_bytes_union() {
    test! {
        FromBytes {
            union Foo {
                a: u8,
            }
        } expands to "expected/from_bytes_union.expected.rs"
    }
}

#[test]
fn test_into_bytes_struct_empty() {
    test! {
        IntoBytes {
            #[repr(C)]
            struct Foo;
        } expands to "expected/into_bytes_struct_empty.expected.rs"
    }
}

#[test]
fn test_into_bytes_struct_basic() {
    test! {
        IntoBytes {
            #[repr(C)]
            struct Foo {
                a: u8,
                b: u8,
            }
        } expands to "expected/into_bytes_struct_basic.expected.rs"
    }
}

#[test]
fn test_into_bytes_struct_trailing() {
    test! {
        IntoBytes {
            #[repr(C)]
            struct Foo {
                a: u8,
                b: [Trailing],
            }
        } expands to "expected/into_bytes_struct_trailing.expected.rs"
    }
}

#[test]
fn test_into_bytes_struct_trailing_generic() {
    test! {
        IntoBytes {
            #[repr(C)]
            struct Foo<Trailing> {
                a: u8,
                b: [Trailing],
            }
        } expands to "expected/into_bytes_struct_trailing_generic.expected.rs"
    }
}

#[test]
fn test_into_bytes_enum() {
    macro_rules! test_repr {
        ($(#[$attr:meta])*) => {
            $(test! {
                IntoBytes {
                    #[$attr]
                    enum Foo {
                        Bar,
                    }
                } expands to concat!("expected/into_bytes_enum.", stringify!($attr), ".expected.rs")
            })*
        };
    }

    test_repr! {
        #[repr(C)]
        #[repr(u8)]
        #[repr(u16)]
        #[repr(u32)]
        #[repr(u64)]
        #[repr(u128)]
        #[repr(usize)]
        #[repr(i8)]
        #[repr(i16)]
        #[repr(i32)]
        #[repr(i64)]
        #[repr(i128)]
        #[repr(isize)]
    }
}

#[test]
fn test_unaligned() {
    test! {
        Unaligned {
            #[repr(C)]
            struct Foo;
        } expands to "expected/unaligned.expected.rs"
    }
}

#[test]
fn test_try_from_bytes_enum() {
    test! {
        TryFromBytes {
            #[repr(u8)]
            enum ComplexWithGenerics<'a: 'static, const N: usize, X, Y: Deref>
            where
                X: Deref<Target = &'a [(X, Y); N]>,
            {
                UnitLike,
                StructLike { a: u8, b: X, c: X::Target, d: Y::Target, e: [(X, Y); N] },
                TupleLike(bool, Y, PhantomData<&'a [(X, Y); N]>),
            }
        } expands to "expected/try_from_bytes_enum_1.expected.rs"
    }

    test! {
        TryFromBytes {
            #[repr(u32)]
            enum ComplexWithGenerics<'a: 'static, const N: usize, X, Y: Deref>
            where
                X: Deref<Target = &'a [(X, Y); N]>,
            {
                UnitLike,
                StructLike { a: u8, b: X, c: X::Target, d: Y::Target, e: [(X, Y); N] },
                TupleLike(bool, Y, PhantomData<&'a [(X, Y); N]>),
            }
        } expands to "expected/try_from_bytes_enum_2.expected.rs"
    }

    test! {
        TryFromBytes {
            #[repr(C)]
            enum ComplexWithGenerics<'a: 'static, const N: usize, X, Y: Deref>
            where
                X: Deref<Target = &'a [(X, Y); N]>,
            {
                UnitLike,
                StructLike { a: u8, b: X, c: X::Target, d: Y::Target, e: [(X, Y); N] },
                TupleLike(bool, Y, PhantomData<&'a [(X, Y); N]>),
            }
        } expands to "expected/try_from_bytes_enum_3.expected.rs"
    }
}

// This goes at the bottom because it's so verbose and it makes scrolling past
// other code a pain.
#[test]
fn test_from_bytes_enum() {
    test! {
        FromBytes {
            #[repr(u8)]
            enum Foo {
                Variant0,
                Variant1,
                Variant2,
                Variant3,
                Variant4,
                Variant5,
                Variant6,
                Variant7,
                Variant8,
                Variant9,
                Variant10,
                Variant11,
                Variant12,
                Variant13,
                Variant14,
                Variant15,
                Variant16,
                Variant17,
                Variant18,
                Variant19,
                Variant20,
                Variant21,
                Variant22,
                Variant23,
                Variant24,
                Variant25,
                Variant26,
                Variant27,
                Variant28,
                Variant29,
                Variant30,
                Variant31,
                Variant32,
                Variant33,
                Variant34,
                Variant35,
                Variant36,
                Variant37,
                Variant38,
                Variant39,
                Variant40,
                Variant41,
                Variant42,
                Variant43,
                Variant44,
                Variant45,
                Variant46,
                Variant47,
                Variant48,
                Variant49,
                Variant50,
                Variant51,
                Variant52,
                Variant53,
                Variant54,
                Variant55,
                Variant56,
                Variant57,
                Variant58,
                Variant59,
                Variant60,
                Variant61,
                Variant62,
                Variant63,
                Variant64,
                Variant65,
                Variant66,
                Variant67,
                Variant68,
                Variant69,
                Variant70,
                Variant71,
                Variant72,
                Variant73,
                Variant74,
                Variant75,
                Variant76,
                Variant77,
                Variant78,
                Variant79,
                Variant80,
                Variant81,
                Variant82,
                Variant83,
                Variant84,
                Variant85,
                Variant86,
                Variant87,
                Variant88,
                Variant89,
                Variant90,
                Variant91,
                Variant92,
                Variant93,
                Variant94,
                Variant95,
                Variant96,
                Variant97,
                Variant98,
                Variant99,
                Variant100,
                Variant101,
                Variant102,
                Variant103,
                Variant104,
                Variant105,
                Variant106,
                Variant107,
                Variant108,
                Variant109,
                Variant110,
                Variant111,
                Variant112,
                Variant113,
                Variant114,
                Variant115,
                Variant116,
                Variant117,
                Variant118,
                Variant119,
                Variant120,
                Variant121,
                Variant122,
                Variant123,
                Variant124,
                Variant125,
                Variant126,
                Variant127,
                Variant128,
                Variant129,
                Variant130,
                Variant131,
                Variant132,
                Variant133,
                Variant134,
                Variant135,
                Variant136,
                Variant137,
                Variant138,
                Variant139,
                Variant140,
                Variant141,
                Variant142,
                Variant143,
                Variant144,
                Variant145,
                Variant146,
                Variant147,
                Variant148,
                Variant149,
                Variant150,
                Variant151,
                Variant152,
                Variant153,
                Variant154,
                Variant155,
                Variant156,
                Variant157,
                Variant158,
                Variant159,
                Variant160,
                Variant161,
                Variant162,
                Variant163,
                Variant164,
                Variant165,
                Variant166,
                Variant167,
                Variant168,
                Variant169,
                Variant170,
                Variant171,
                Variant172,
                Variant173,
                Variant174,
                Variant175,
                Variant176,
                Variant177,
                Variant178,
                Variant179,
                Variant180,
                Variant181,
                Variant182,
                Variant183,
                Variant184,
                Variant185,
                Variant186,
                Variant187,
                Variant188,
                Variant189,
                Variant190,
                Variant191,
                Variant192,
                Variant193,
                Variant194,
                Variant195,
                Variant196,
                Variant197,
                Variant198,
                Variant199,
                Variant200,
                Variant201,
                Variant202,
                Variant203,
                Variant204,
                Variant205,
                Variant206,
                Variant207,
                Variant208,
                Variant209,
                Variant210,
                Variant211,
                Variant212,
                Variant213,
                Variant214,
                Variant215,
                Variant216,
                Variant217,
                Variant218,
                Variant219,
                Variant220,
                Variant221,
                Variant222,
                Variant223,
                Variant224,
                Variant225,
                Variant226,
                Variant227,
                Variant228,
                Variant229,
                Variant230,
                Variant231,
                Variant232,
                Variant233,
                Variant234,
                Variant235,
                Variant236,
                Variant237,
                Variant238,
                Variant239,
                Variant240,
                Variant241,
                Variant242,
                Variant243,
                Variant244,
                Variant245,
                Variant246,
                Variant247,
                Variant248,
                Variant249,
                Variant250,
                Variant251,
                Variant252,
                Variant253,
                Variant254,
                Variant255,
            }
        } expands to "expected/from_bytes_enum.expected.rs"
    }
}

#[test]
fn test_try_from_bytes_trivial_is_bit_valid_enum() {
    // Even when we aren't deriving `FromBytes` as the top-level trait,
    // `TryFromBytes` on enums still detects whether we *could* derive
    // `FromBytes`, and if so, performs the same "trivial `is_bit_valid`"
    // optimization.
    test! {
        TryFromBytes {
            #[repr(u8)]
            enum Foo {
                Variant0,
                Variant1,
                Variant2,
                Variant3,
                Variant4,
                Variant5,
                Variant6,
                Variant7,
                Variant8,
                Variant9,
                Variant10,
                Variant11,
                Variant12,
                Variant13,
                Variant14,
                Variant15,
                Variant16,
                Variant17,
                Variant18,
                Variant19,
                Variant20,
                Variant21,
                Variant22,
                Variant23,
                Variant24,
                Variant25,
                Variant26,
                Variant27,
                Variant28,
                Variant29,
                Variant30,
                Variant31,
                Variant32,
                Variant33,
                Variant34,
                Variant35,
                Variant36,
                Variant37,
                Variant38,
                Variant39,
                Variant40,
                Variant41,
                Variant42,
                Variant43,
                Variant44,
                Variant45,
                Variant46,
                Variant47,
                Variant48,
                Variant49,
                Variant50,
                Variant51,
                Variant52,
                Variant53,
                Variant54,
                Variant55,
                Variant56,
                Variant57,
                Variant58,
                Variant59,
                Variant60,
                Variant61,
                Variant62,
                Variant63,
                Variant64,
                Variant65,
                Variant66,
                Variant67,
                Variant68,
                Variant69,
                Variant70,
                Variant71,
                Variant72,
                Variant73,
                Variant74,
                Variant75,
                Variant76,
                Variant77,
                Variant78,
                Variant79,
                Variant80,
                Variant81,
                Variant82,
                Variant83,
                Variant84,
                Variant85,
                Variant86,
                Variant87,
                Variant88,
                Variant89,
                Variant90,
                Variant91,
                Variant92,
                Variant93,
                Variant94,
                Variant95,
                Variant96,
                Variant97,
                Variant98,
                Variant99,
                Variant100,
                Variant101,
                Variant102,
                Variant103,
                Variant104,
                Variant105,
                Variant106,
                Variant107,
                Variant108,
                Variant109,
                Variant110,
                Variant111,
                Variant112,
                Variant113,
                Variant114,
                Variant115,
                Variant116,
                Variant117,
                Variant118,
                Variant119,
                Variant120,
                Variant121,
                Variant122,
                Variant123,
                Variant124,
                Variant125,
                Variant126,
                Variant127,
                Variant128,
                Variant129,
                Variant130,
                Variant131,
                Variant132,
                Variant133,
                Variant134,
                Variant135,
                Variant136,
                Variant137,
                Variant138,
                Variant139,
                Variant140,
                Variant141,
                Variant142,
                Variant143,
                Variant144,
                Variant145,
                Variant146,
                Variant147,
                Variant148,
                Variant149,
                Variant150,
                Variant151,
                Variant152,
                Variant153,
                Variant154,
                Variant155,
                Variant156,
                Variant157,
                Variant158,
                Variant159,
                Variant160,
                Variant161,
                Variant162,
                Variant163,
                Variant164,
                Variant165,
                Variant166,
                Variant167,
                Variant168,
                Variant169,
                Variant170,
                Variant171,
                Variant172,
                Variant173,
                Variant174,
                Variant175,
                Variant176,
                Variant177,
                Variant178,
                Variant179,
                Variant180,
                Variant181,
                Variant182,
                Variant183,
                Variant184,
                Variant185,
                Variant186,
                Variant187,
                Variant188,
                Variant189,
                Variant190,
                Variant191,
                Variant192,
                Variant193,
                Variant194,
                Variant195,
                Variant196,
                Variant197,
                Variant198,
                Variant199,
                Variant200,
                Variant201,
                Variant202,
                Variant203,
                Variant204,
                Variant205,
                Variant206,
                Variant207,
                Variant208,
                Variant209,
                Variant210,
                Variant211,
                Variant212,
                Variant213,
                Variant214,
                Variant215,
                Variant216,
                Variant217,
                Variant218,
                Variant219,
                Variant220,
                Variant221,
                Variant222,
                Variant223,
                Variant224,
                Variant225,
                Variant226,
                Variant227,
                Variant228,
                Variant229,
                Variant230,
                Variant231,
                Variant232,
                Variant233,
                Variant234,
                Variant235,
                Variant236,
                Variant237,
                Variant238,
                Variant239,
                Variant240,
                Variant241,
                Variant242,
                Variant243,
                Variant244,
                Variant245,
                Variant246,
                Variant247,
                Variant248,
                Variant249,
                Variant250,
                Variant251,
                Variant252,
                Variant253,
                Variant254,
                Variant255,
            }
        } expands to "expected/try_from_bytes_trivial_is_bit_valid_enum.expected.rs"
    }
}

#[test]
fn test_hash() {
    test! {
        ByteHash {
            struct Foo<T: Clone>(T) where Self: Sized;
        } expands to "expected/hash.expected.rs"
    }
}

#[test]
fn test_eq() {
    test! {
        ByteEq {
            struct Foo<T: Clone>(T) where Self: Sized;
        } expands to "expected/eq.expected.rs"
    }
}

#[test]
fn test_split_at() {
    test! {
        SplitAt {
            #[repr(C)]
            struct Foo<T: ?Sized + Copy>(T) where Self: Copy;
        } expands to "expected/split_at_repr_c.expected.rs"
    }

    test! {
        SplitAt {
            #[repr(transparent)]
            struct Foo<T: ?Sized + Copy>(T) where Self: Copy;
        } expands to "expected/split_at_repr_transparent.expected.rs"
    }

    test! {
        SplitAt {
            #[repr(packed)]
            struct Foo<T: ?Sized + Copy>(T) where Self: Copy;
        } expands to {
            ::core::compile_error! {
                "must not have #[repr(packed)] attribute"
            }
        } no_build
    }

    test! {
        SplitAt {
            #[repr(packed(2))]
            struct Foo<T: ?Sized + Copy>(T) where Self: Copy;
        } expands to {
            ::core::compile_error! {
                "must not have #[repr(packed)] attribute"
            }
        } no_build
    }

    test! {
        SplitAt {
            enum Foo {}
        } expands to {
            ::core::compile_error! {
                "can only be applied to structs"
            }
        } no_build
    }

    test! {
        SplitAt {
            union Foo { a: () }
        } expands to {
            ::core::compile_error! {
                "can only be applied to structs"
            }
        } no_build
    }

    test! {
        SplitAt {
            struct Foo<T: ?Sized + Copy>(T) where Self: Copy;
        } expands to {
            ::core::compile_error! {
                "must have #[repr(C)] or #[repr(transparent)] in order to guarantee this type's layout is splitable"
            }
        } no_build
    }
}
