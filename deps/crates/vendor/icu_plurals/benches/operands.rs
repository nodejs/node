// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

mod fixtures;

use criterion::{black_box, criterion_group, criterion_main, Criterion};
use fixed_decimal::Decimal;
use icu_plurals::PluralOperands;

fn operands(c: &mut Criterion) {
    let data =
        serde_json::from_str::<fixtures::NumbersFixture>(include_str!("fixtures/numbers.json"))
            .expect("Failed to read a fixture");

    c.bench_function("plurals/operands/overview", |b| {
        b.iter(|| {
            for s in &data.usize {
                let _: PluralOperands = black_box(*s).into();
            }
            for s in &data.isize {
                let _: PluralOperands = black_box(*s).into();
            }
            for s in &data.string {
                let _: PluralOperands = black_box(s)
                    .parse()
                    .expect("Failed to parse a number into an operands.");
            }
            for s in &data.fixed_decimals {
                let f: Decimal = {
                    let mut dec = Decimal::from(s.value);
                    dec.multiply_pow10(s.exponent);
                    dec
                };
                let _: PluralOperands = PluralOperands::from(black_box(&f));
            }
        })
    });

    {
        use criterion::BenchmarkId;

        c.bench_function("plurals/operands/create/usize", |b| {
            b.iter(|| {
                for s in &data.usize {
                    let _: PluralOperands = black_box(*s).into();
                }
            })
        });

        c.bench_function("plurals/operands/create/isize", |b| {
            b.iter(|| {
                for s in &data.isize {
                    let _: PluralOperands = black_box(*s).into();
                }
            })
        });

        c.bench_function("plurals/operands/create/string", |b| {
            b.iter(|| {
                for s in &data.string {
                    let _: PluralOperands = black_box(s)
                        .parse()
                        .expect("Failed to parse a number into an operands.");
                }
            })
        });

        {
            let mut group = c.benchmark_group("plurals/operands/create/string/samples");
            for s in &data.string_samples {
                group.bench_with_input(BenchmarkId::from_parameter(s), s, |b, s| {
                    b.iter(|| {
                        let _: PluralOperands = black_box(s).parse().unwrap();
                    })
                });
            }
        }
        c.bench_function("plurals/operands/eq/mostly_unequal", |b| {
            let p: PluralOperands = 1.into();
            b.iter(|| {
                for s in &data.isize {
                    let q: PluralOperands = black_box(*s).into();
                    let _ = black_box(black_box(p) == black_box(q));
                }
            })
        });

        c.bench_function("plurals/operands/eq/mostly_equal", |b| {
            b.iter(|| {
                for s in &data.isize {
                    let p: PluralOperands = black_box(*s).into();
                    let q: PluralOperands = black_box(*s).into();
                    let _ = black_box(black_box(p) == black_box(q));
                }
            })
        });

        c.bench_function("plurals/operands/create/from_fixed_decimal", |b| {
            b.iter(|| {
                for s in &data.fixed_decimals {
                    let f: Decimal = {
                        let mut dec = Decimal::from(s.value);
                        dec.multiply_pow10(s.exponent);
                        dec
                    };
                    let _: PluralOperands = PluralOperands::from(black_box(&f));
                }
            });
        });

        {
            let samples = [
                {
                    let mut dec = Decimal::from(1_i128);
                    dec.multiply_pow10(0);
                    dec
                },
                {
                    let mut dec = Decimal::from(123450_i128);
                    dec.multiply_pow10(-4);
                    dec
                },
                {
                    let mut dec = Decimal::from(2500_i128);
                    dec.multiply_pow10(-2);
                    dec
                },
            ];
            let mut group = c.benchmark_group("plurals/operands/create/from_fixed_decimal/samples");
            for s in samples.iter() {
                group.bench_with_input(
                    BenchmarkId::from_parameter(format!("{:?}", &s)),
                    s,
                    |b, f| b.iter(|| PluralOperands::from(black_box(f))),
                );
            }
        }
    }
}

criterion_group!(benches, operands);
criterion_main!(benches);
