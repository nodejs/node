use core::f64::consts::LOG2_10;
use core::mem;
use num_bigint::BigUint as Uint;

const _: () = {
    let static_data =
        mem::size_of_val(&crate::POW10_SIGNIFICANDS) + mem::size_of_val(&crate::DIGITS2);
    if crate::Pow10SignificandsTable::COMPRESS {
        assert!(static_data == 200);
    } else {
        assert!(static_data == 10120); // 9.9K
    }
};

#[cfg(target_endian = "little")]
#[test]
fn utilities() {
    let clz = u64::leading_zeros;
    assert_eq!(clz(1), 63);
    assert_eq!(clz(!0), 0);

    assert_eq!(crate::count_trailing_nonzeros(0x00000000_00000000), 0);
    assert_eq!(crate::count_trailing_nonzeros(0x00000000_00000001), 1);
    assert_eq!(crate::count_trailing_nonzeros(0x00000000_00000009), 1);
    assert_eq!(crate::count_trailing_nonzeros(0x00090000_09000000), 7);
    assert_eq!(crate::count_trailing_nonzeros(0x01000000_00000000), 8);
    assert_eq!(crate::count_trailing_nonzeros(0x09000000_00000000), 8);
}

#[test]
fn umulhi_inexact_to_odd() {
    let pow10 = crate::POW10_SIGNIFICANDS.get(-292);
    assert_eq!(
        crate::umulhi_inexact_to_odd(pow10.hi, pow10.lo, 0x1234567890abcdefu64 << 1),
        0x24554a3ce60a45f5,
    );
    assert_eq!(
        crate::umulhi_inexact_to_odd(pow10.hi, pow10.lo, 0x1234567890abce16u64 << 1),
        0x24554a3ce60a4643,
    );
}

#[test]
fn pow10() {
    const DEC_EXP_MIN: i32 = -292;

    // Range of decimal exponents [K_min, K_max] from the paper.
    let dec_exp_min = -324_i32;
    let dec_exp_max = 292_i32;

    let num_bits = 128_i32;

    // Negate dec_pow_min and dec_pow_max because we need negative powers 10^-k.
    for (i, dec_exp) in (-dec_exp_max..=-dec_exp_min).enumerate() {
        // dec_exp is -k in the paper.
        let bin_exp = (f64::from(dec_exp) * LOG2_10).floor() as i32 - (num_bits - 1);
        let bin_pow = Uint::from(2_u8).pow(bin_exp.unsigned_abs());
        let dec_pow = Uint::from(10_u8).pow(dec_exp.unsigned_abs());
        let result = if dec_exp < 0 {
            bin_pow / dec_pow
        } else if bin_exp < 0 {
            dec_pow * bin_pow
        } else {
            dec_pow / bin_pow
        };
        let hi = u64::try_from(&result >> 64).unwrap();
        let lo = u64::try_from(result & (Uint::from(2_u8).pow(64) - Uint::from(1_u8))).unwrap();
        if !crate::Pow10SignificandsTable::COMPRESS {
            assert_eq!(
                crate::POW10_SIGNIFICANDS.get(DEC_EXP_MIN + i as i32),
                crate::uint128 { hi, lo },
            );
        }
    }
}
