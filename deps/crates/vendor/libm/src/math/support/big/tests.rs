extern crate std;
use std::string::String;
use std::{eprintln, format};

use super::{HInt, MinInt, i256, u256};

const LOHI_SPLIT: u128 = 0xaaaaaaaaaaaaaaaaffffffffffffffff;

/// Print a `u256` as hex since we can't add format implementations
fn hexu(v: u256) -> String {
    format!("0x{:032x}{:032x}", v.hi, v.lo)
}

#[test]
fn widen_u128() {
    assert_eq!(
        u128::MAX.widen(),
        u256 {
            lo: u128::MAX,
            hi: 0
        }
    );
    assert_eq!(
        LOHI_SPLIT.widen(),
        u256 {
            lo: LOHI_SPLIT,
            hi: 0
        }
    );
}

#[test]
fn widen_i128() {
    assert_eq!((-1i128).widen(), u256::MAX.signed());
    assert_eq!(
        (LOHI_SPLIT as i128).widen(),
        i256 {
            lo: LOHI_SPLIT,
            hi: u128::MAX
        }
    );
    assert_eq!((-1i128).zero_widen().unsigned(), (u128::MAX).widen());
}

#[test]
fn widen_mul_u128() {
    let tests = [
        (
            u128::MAX / 2,
            2_u128,
            u256 {
                lo: u128::MAX - 1,
                hi: 0,
            },
        ),
        (
            u128::MAX,
            2_u128,
            u256 {
                lo: u128::MAX - 1,
                hi: 1,
            },
        ),
        (
            u128::MAX,
            u128::MAX,
            u256 {
                lo: 1,
                hi: u128::MAX - 1,
            },
        ),
        (0, 0, u256::ZERO),
        (1234u128, 0, u256::ZERO),
        (0, 1234, u256::ZERO),
    ];

    let mut has_errors = false;
    let mut add_error = |i, a, b, expected, actual| {
        has_errors = true;
        eprintln!(
            "\
            FAILURE ({i}): {a:#034x} * {b:#034x}\n\
            expected: {}\n\
            got:      {}\
            ",
            hexu(expected),
            hexu(actual)
        );
    };

    for (i, (a, b, exp)) in tests.iter().copied().enumerate() {
        let res = a.widen_mul(b);
        let res_z = a.zero_widen_mul(b);
        assert_eq!(res, res_z);
        if res != exp {
            add_error(i, a, b, exp, res);
        }
    }

    assert!(!has_errors);
}

#[test]
fn not_u256() {
    assert_eq!(!u256::ZERO, u256::MAX);
}

#[test]
fn shr_u256() {
    let only_low = [
        1,
        u16::MAX.into(),
        u32::MAX.into(),
        u64::MAX.into(),
        u128::MAX,
    ];
    let mut has_errors = false;

    let mut add_error = |a, b, expected, actual| {
        has_errors = true;
        eprintln!(
            "\
            FAILURE:  {} >> {b}\n\
            expected: {}\n\
            actual:   {}\
            ",
            hexu(a),
            hexu(expected),
            hexu(actual),
        );
    };

    for a in only_low {
        for perturb in 0..10 {
            let a = a.saturating_add(perturb);
            for shift in 0..128 {
                let res = a.widen() >> shift;
                let expected = (a >> shift).widen();
                if res != expected {
                    add_error(a.widen(), shift, expected, res);
                }
            }
        }
    }

    let check = [
        (
            u256::MAX,
            1,
            u256 {
                lo: u128::MAX,
                hi: u128::MAX >> 1,
            },
        ),
        (
            u256::MAX,
            5,
            u256 {
                lo: u128::MAX,
                hi: u128::MAX >> 5,
            },
        ),
        (
            u256::MAX,
            63,
            u256 {
                lo: u128::MAX,
                hi: u64::MAX as u128 | (1 << 64),
            },
        ),
        (
            u256::MAX,
            64,
            u256 {
                lo: u128::MAX,
                hi: u64::MAX as u128,
            },
        ),
        (
            u256::MAX,
            65,
            u256 {
                lo: u128::MAX,
                hi: (u64::MAX >> 1) as u128,
            },
        ),
        (
            u256::MAX,
            127,
            u256 {
                lo: u128::MAX,
                hi: 1,
            },
        ),
        (
            u256::MAX,
            128,
            u256 {
                lo: u128::MAX,
                hi: 0,
            },
        ),
        (
            u256::MAX,
            129,
            u256 {
                lo: u128::MAX >> 1,
                hi: 0,
            },
        ),
        (
            u256::MAX,
            191,
            u256 {
                lo: u64::MAX as u128 | 1 << 64,
                hi: 0,
            },
        ),
        (
            u256::MAX,
            192,
            u256 {
                lo: u64::MAX as u128,
                hi: 0,
            },
        ),
        (
            u256::MAX,
            193,
            u256 {
                lo: u64::MAX as u128 >> 1,
                hi: 0,
            },
        ),
        (u256::MAX, 254, u256 { lo: 0b11, hi: 0 }),
        (u256::MAX, 255, u256 { lo: 1, hi: 0 }),
        (
            u256 {
                hi: LOHI_SPLIT,
                lo: 0,
            },
            64,
            u256 {
                lo: 0xffffffffffffffff0000000000000000,
                hi: 0xaaaaaaaaaaaaaaaa,
            },
        ),
    ];

    for (input, shift, expected) in check {
        let res = input >> shift;
        if res != expected {
            add_error(input, shift, expected, res);
        }
    }

    assert!(!has_errors);
}

#[test]
#[should_panic]
#[cfg(debug_assertions)]
// FIXME(ppc): ppc64le seems to have issues with `should_panic` tests.
#[cfg(not(all(target_arch = "powerpc64", target_endian = "little")))]
fn shr_u256_overflow() {
    // Like regular shr, panic on overflow with debug assertions
    let _ = u256::MAX >> 256;
}

#[test]
#[cfg(not(debug_assertions))]
fn shr_u256_overflow() {
    // No panic without debug assertions
    assert_eq!(u256::MAX >> 256, u256::ZERO);
    assert_eq!(u256::MAX >> 257, u256::ZERO);
    assert_eq!(u256::MAX >> u32::MAX, u256::ZERO);
}
