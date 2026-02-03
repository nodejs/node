//! Utilities for working with hex float formats.

use core::fmt;

use super::{Float, Round, Status, f32_from_bits, f64_from_bits};

/// Construct a 16-bit float from hex float representation (C-style)
#[cfg(f16_enabled)]
pub const fn hf16(s: &str) -> f16 {
    match parse_hex_exact(s, 16, 10) {
        Ok(bits) => f16::from_bits(bits as u16),
        Err(HexFloatParseError(s)) => panic!("{}", s),
    }
}

/// Construct a 32-bit float from hex float representation (C-style)
#[allow(unused)]
pub const fn hf32(s: &str) -> f32 {
    match parse_hex_exact(s, 32, 23) {
        Ok(bits) => f32_from_bits(bits as u32),
        Err(HexFloatParseError(s)) => panic!("{}", s),
    }
}

/// Construct a 64-bit float from hex float representation (C-style)
pub const fn hf64(s: &str) -> f64 {
    match parse_hex_exact(s, 64, 52) {
        Ok(bits) => f64_from_bits(bits as u64),
        Err(HexFloatParseError(s)) => panic!("{}", s),
    }
}

/// Construct a 128-bit float from hex float representation (C-style)
#[cfg(f128_enabled)]
pub const fn hf128(s: &str) -> f128 {
    match parse_hex_exact(s, 128, 112) {
        Ok(bits) => f128::from_bits(bits),
        Err(HexFloatParseError(s)) => panic!("{}", s),
    }
}
#[derive(Copy, Clone, Debug)]
pub struct HexFloatParseError(&'static str);

/// Parses any float to its bitwise representation, returning an error if it cannot be represented exactly
pub const fn parse_hex_exact(
    s: &str,
    bits: u32,
    sig_bits: u32,
) -> Result<u128, HexFloatParseError> {
    match parse_any(s, bits, sig_bits, Round::Nearest) {
        Err(e) => Err(e),
        Ok((bits, Status::OK)) => Ok(bits),
        Ok((_, status)) if status.overflow() => Err(HexFloatParseError("the value is too huge")),
        Ok((_, status)) if status.underflow() => Err(HexFloatParseError("the value is too tiny")),
        Ok((_, status)) if status.inexact() => Err(HexFloatParseError("the value is too precise")),
        Ok(_) => unreachable!(),
    }
}

/// Parse any float from hex to its bitwise representation.
pub const fn parse_any(
    s: &str,
    bits: u32,
    sig_bits: u32,
    round: Round,
) -> Result<(u128, Status), HexFloatParseError> {
    let mut b = s.as_bytes();

    if sig_bits > 119 || bits > 128 || bits < sig_bits + 3 || bits > sig_bits + 30 {
        return Err(HexFloatParseError("unsupported target float configuration"));
    }

    let neg = matches!(b, [b'-', ..]);
    if let &[b'-' | b'+', ref rest @ ..] = b {
        b = rest;
    }

    let sign_bit = 1 << (bits - 1);
    let quiet_bit = 1 << (sig_bits - 1);
    let nan = sign_bit - quiet_bit;
    let inf = nan - quiet_bit;

    let (mut x, status) = match *b {
        [b'i' | b'I', b'n' | b'N', b'f' | b'F'] => (inf, Status::OK),
        [b'n' | b'N', b'a' | b'A', b'n' | b'N'] => (nan, Status::OK),
        [b'0', b'x' | b'X', ref rest @ ..] => {
            let round = match (neg, round) {
                // parse("-x", Round::Positive) == -parse("x", Round::Negative)
                (true, Round::Positive) => Round::Negative,
                (true, Round::Negative) => Round::Positive,
                // rounding toward nearest or zero are symmetric
                (true, Round::Nearest | Round::Zero) | (false, _) => round,
            };
            match parse_finite(rest, bits, sig_bits, round) {
                Err(e) => return Err(e),
                Ok(res) => res,
            }
        }
        _ => return Err(HexFloatParseError("no hex indicator")),
    };

    if neg {
        x ^= sign_bit;
    }

    Ok((x, status))
}

const fn parse_finite(
    b: &[u8],
    bits: u32,
    sig_bits: u32,
    rounding_mode: Round,
) -> Result<(u128, Status), HexFloatParseError> {
    let exp_bits: u32 = bits - sig_bits - 1;
    let max_msb: i32 = (1 << (exp_bits - 1)) - 1;
    // The exponent of one ULP in the subnormals
    let min_lsb: i32 = 1 - max_msb - sig_bits as i32;

    let (mut sig, mut exp) = match parse_hex(b) {
        Err(e) => return Err(e),
        Ok(Parsed { sig: 0, .. }) => return Ok((0, Status::OK)),
        Ok(Parsed { sig, exp }) => (sig, exp),
    };

    let mut round_bits = u128_ilog2(sig) as i32 - sig_bits as i32;

    // Round at least up to min_lsb
    if exp < min_lsb - round_bits {
        round_bits = min_lsb - exp;
    }

    let mut status = Status::OK;

    exp += round_bits;

    if round_bits > 0 {
        // first, prepare for rounding exactly two bits
        if round_bits == 1 {
            sig <<= 1;
        } else if round_bits > 2 {
            sig = shr_odd_rounding(sig, (round_bits - 2) as u32);
        }

        if sig & 0b11 != 0 {
            status = Status::INEXACT;
        }

        sig = shr2_round(sig, rounding_mode);
    } else if round_bits < 0 {
        sig <<= -round_bits;
    }

    // The parsed value is X = sig * 2^exp
    // Expressed as a multiple U of the smallest subnormal value:
    // X = U * 2^min_lsb, so U = sig * 2^(exp-min_lsb)
    let uexp = (exp - min_lsb) as u128;
    let uexp = uexp << sig_bits;

    // Note that it is possible for the exponent bits to equal 2 here
    // if the value rounded up, but that means the mantissa is all zeroes
    // so the value is still correct
    debug_assert!(sig <= 2 << sig_bits);

    let inf = ((1 << exp_bits) - 1) << sig_bits;

    let bits = match sig.checked_add(uexp) {
        Some(bits) if bits < inf => {
            // inexact subnormal or zero?
            if status.inexact() && bits < (1 << sig_bits) {
                status = status.with(Status::UNDERFLOW);
            }
            bits
        }
        _ => {
            // overflow to infinity
            status = status.with(Status::OVERFLOW).with(Status::INEXACT);
            match rounding_mode {
                Round::Positive | Round::Nearest => inf,
                Round::Negative | Round::Zero => inf - 1,
            }
        }
    };
    Ok((bits, status))
}

/// Shift right, rounding all inexact divisions to the nearest odd number
/// E.g. (0 >> 4) -> 0, (1..=31 >> 4) -> 1, (32 >> 4) -> 2, ...
///
/// Useful for reducing a number before rounding the last two bits, since
/// the result of the final rounding is preserved for all rounding modes.
const fn shr_odd_rounding(x: u128, k: u32) -> u128 {
    if k < 128 {
        let inexact = x.trailing_zeros() < k;
        (x >> k) | (inexact as u128)
    } else {
        (x != 0) as u128
    }
}

/// Divide by 4, rounding with the given mode
const fn shr2_round(mut x: u128, round: Round) -> u128 {
    let t = (x as u32) & 0b111;
    x >>= 2;
    match round {
        // Look-up-table on the last three bits for when to round up
        Round::Nearest => x + ((0b11001000_u8 >> t) & 1) as u128,

        Round::Negative => x,
        Round::Zero => x,
        Round::Positive => x + (t & 0b11 != 0) as u128,
    }
}

/// A parsed finite and unsigned floating point number.
struct Parsed {
    /// Absolute value sig * 2^exp
    sig: u128,
    exp: i32,
}

/// Parse a hexadecimal float x
const fn parse_hex(mut b: &[u8]) -> Result<Parsed, HexFloatParseError> {
    let mut sig: u128 = 0;
    let mut exp: i32 = 0;

    let mut seen_point = false;
    let mut some_digits = false;
    let mut inexact = false;

    while let &[c, ref rest @ ..] = b {
        b = rest;

        match c {
            b'.' => {
                if seen_point {
                    return Err(HexFloatParseError(
                        "unexpected '.' parsing fractional digits",
                    ));
                }
                seen_point = true;
                continue;
            }
            b'p' | b'P' => break,
            c => {
                let digit = match hex_digit(c) {
                    Some(d) => d,
                    None => return Err(HexFloatParseError("expected hexadecimal digit")),
                };
                some_digits = true;

                if (sig >> 124) == 0 {
                    sig <<= 4;
                    sig |= digit as u128;
                } else {
                    // FIXME: it is technically possible for exp to overflow if parsing a string with >500M digits
                    exp += 4;
                    inexact |= digit != 0;
                }
                // Up until the fractional point, the value grows
                // with more digits, but after it the exponent is
                // compensated to match.
                if seen_point {
                    exp -= 4;
                }
            }
        }
    }
    // If we've set inexact, the exact value has more than 125
    // significant bits, and lies somewhere between sig and sig + 1.
    // Because we'll round off at least two of the trailing bits,
    // setting the last bit gives correct rounding for inexact values.
    sig |= inexact as u128;

    if !some_digits {
        return Err(HexFloatParseError("at least one digit is required"));
    };

    some_digits = false;

    let negate_exp = matches!(b, [b'-', ..]);
    if let &[b'-' | b'+', ref rest @ ..] = b {
        b = rest;
    }

    let mut pexp: u32 = 0;
    while let &[c, ref rest @ ..] = b {
        b = rest;
        let digit = match dec_digit(c) {
            Some(d) => d,
            None => return Err(HexFloatParseError("expected decimal digit")),
        };
        some_digits = true;
        pexp = pexp.saturating_mul(10);
        pexp += digit as u32;
    }

    if !some_digits {
        return Err(HexFloatParseError(
            "at least one exponent digit is required",
        ));
    };

    {
        let e;
        if negate_exp {
            e = (exp as i64) - (pexp as i64);
        } else {
            e = (exp as i64) + (pexp as i64);
        };

        exp = if e < i32::MIN as i64 {
            i32::MIN
        } else if e > i32::MAX as i64 {
            i32::MAX
        } else {
            e as i32
        };
    }
    /* FIXME(msrv): once MSRV >= 1.66, replace the above workaround block with:
    if negate_exp {
        exp = exp.saturating_sub_unsigned(pexp);
    } else {
        exp = exp.saturating_add_unsigned(pexp);
    };
    */

    Ok(Parsed { sig, exp })
}

const fn dec_digit(c: u8) -> Option<u8> {
    match c {
        b'0'..=b'9' => Some(c - b'0'),
        _ => None,
    }
}

const fn hex_digit(c: u8) -> Option<u8> {
    match c {
        b'0'..=b'9' => Some(c - b'0'),
        b'a'..=b'f' => Some(c - b'a' + 10),
        b'A'..=b'F' => Some(c - b'A' + 10),
        _ => None,
    }
}

/* FIXME(msrv): vendor some things that are not const stable at our MSRV */

/// `u128::ilog2`
const fn u128_ilog2(v: u128) -> u32 {
    assert!(v != 0);
    u128::BITS - 1 - v.leading_zeros()
}

/// Format a floating point number as its IEEE hex (`%a`) representation.
pub struct Hexf<F>(pub F);

// Adapted from https://github.com/ericseppanen/hexfloat2/blob/a5c27932f0ff/src/format.rs
#[cfg(not(feature = "compiler-builtins"))]
fn fmt_any_hex<F: Float>(x: &F, f: &mut fmt::Formatter<'_>) -> fmt::Result {
    if x.is_sign_negative() {
        write!(f, "-")?;
    }

    if x.is_nan() {
        return write!(f, "NaN");
    } else if x.is_infinite() {
        return write!(f, "inf");
    } else if *x == F::ZERO {
        return write!(f, "0x0p+0");
    }

    let mut exponent = x.exp_unbiased();
    let sig = x.to_bits() & F::SIG_MASK;

    let bias = F::EXP_BIAS as i32;
    // The mantissa MSB needs to be shifted up to the nearest nibble.
    let mshift = (4 - (F::SIG_BITS % 4)) % 4;
    let sig = sig << mshift;
    // The width is rounded up to the nearest char (4 bits)
    let mwidth = (F::SIG_BITS as usize + 3) / 4;
    let leading = if exponent == -bias {
        // subnormal number means we shift our output by 1 bit.
        exponent += 1;
        "0."
    } else {
        "1."
    };

    write!(f, "0x{leading}{sig:0mwidth$x}p{exponent:+}")
}

#[cfg(feature = "compiler-builtins")]
fn fmt_any_hex<F: Float>(_x: &F, _f: &mut fmt::Formatter<'_>) -> fmt::Result {
    unimplemented!()
}

impl<F: Float> fmt::LowerHex for Hexf<F> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        cfg_if! {
            if #[cfg(feature = "compiler-builtins")] {
                let _ = f;
                unimplemented!()
            } else {
                fmt_any_hex(&self.0, f)
            }
        }
    }
}

impl<F: Float> fmt::LowerHex for Hexf<(F, F)> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        cfg_if! {
            if #[cfg(feature = "compiler-builtins")] {
                let _ = f;
                unimplemented!()
            } else {
                write!(f, "({:x}, {:x})", Hexf(self.0.0), Hexf(self.0.1))
            }
        }
    }
}

impl<F: Float> fmt::LowerHex for Hexf<(F, i32)> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        cfg_if! {
            if #[cfg(feature = "compiler-builtins")] {
                let _ = f;
                unimplemented!()
            } else {
                write!(f, "({:x}, {:x})", Hexf(self.0.0), Hexf(self.0.1))
            }
        }
    }
}

impl fmt::LowerHex for Hexf<i32> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        cfg_if! {
            if #[cfg(feature = "compiler-builtins")] {
                let _ = f;
                unimplemented!()
            } else {
                fmt::LowerHex::fmt(&self.0, f)
            }
        }
    }
}

impl<T> fmt::Debug for Hexf<T>
where
    Hexf<T>: fmt::LowerHex,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        cfg_if! {
            if #[cfg(feature = "compiler-builtins")] {
                let _ = f;
                unimplemented!()
            } else {
                fmt::LowerHex::fmt(self, f)
            }
        }
    }
}

impl<T> fmt::Display for Hexf<T>
where
    Hexf<T>: fmt::LowerHex,
{
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        cfg_if! {
            if #[cfg(feature = "compiler-builtins")] {
                let _ = f;
                unimplemented!()
            } else {
                fmt::LowerHex::fmt(self, f)
            }
        }
    }
}

#[cfg(test)]
mod parse_tests {
    extern crate std;
    use std::{format, println};

    use super::*;

    #[cfg(f16_enabled)]
    fn rounding_properties(s: &str) -> Result<(), HexFloatParseError> {
        let (xd, s0) = parse_any(s, 16, 10, Round::Negative)?;
        let (xu, s1) = parse_any(s, 16, 10, Round::Positive)?;
        let (xz, s2) = parse_any(s, 16, 10, Round::Zero)?;
        let (xn, s3) = parse_any(s, 16, 10, Round::Nearest)?;

        // FIXME: A value between the least normal and largest subnormal
        // could have underflow status depend on rounding mode.

        if let Status::OK = s0 {
            // an exact result is the same for all rounding modes
            assert_eq!(s0, s1);
            assert_eq!(s0, s2);
            assert_eq!(s0, s3);

            assert_eq!(xd, xu);
            assert_eq!(xd, xz);
            assert_eq!(xd, xn);
        } else {
            assert!([s0, s1, s2, s3].into_iter().all(Status::inexact));

            let xd = f16::from_bits(xd as u16);
            let xu = f16::from_bits(xu as u16);
            let xz = f16::from_bits(xz as u16);
            let xn = f16::from_bits(xn as u16);

            assert_biteq!(xd.next_up(), xu, "s={s}, xd={xd:?}, xu={xu:?}");

            let signs = [xd, xu, xz, xn].map(f16::is_sign_negative);

            if signs == [true; 4] {
                assert_biteq!(xz, xu);
            } else {
                assert_eq!(signs, [false; 4]);
                assert_biteq!(xz, xd);
            }

            if xn.to_bits() != xd.to_bits() {
                assert_biteq!(xn, xu);
            }
        }
        Ok(())
    }
    #[test]
    #[cfg(f16_enabled)]
    fn test_rounding() {
        let n = 1_i32 << 14;
        for i in -n..n {
            let u = i.rotate_right(11) as u32;
            let s = format!("{}", Hexf(f32::from_bits(u)));
            assert!(rounding_properties(&s).is_ok());
        }
    }

    #[test]
    fn test_parse_any() {
        for k in -149..=127 {
            let s = format!("0x1p{k}");
            let x = hf32(&s);
            let y = if k < 0 {
                0.5f32.powi(-k)
            } else {
                2.0f32.powi(k)
            };
            assert_eq!(x, y);
        }

        let mut s = *b"0x.0000000p-121";
        for e in 0..40 {
            for k in 0..(1 << 15) {
                let expected = f32::from_bits(k) * 2.0f32.powi(e);
                let x = hf32(std::str::from_utf8(&s).unwrap());
                assert_eq!(
                    x.to_bits(),
                    expected.to_bits(),
                    "\
                    e={e}\n\
                    k={k}\n\
                    x={x}\n\
                    expected={expected}\n\
                    s={}\n\
                    f32::from_bits(k)={}\n\
                    2.0f32.powi(e)={}\
                    ",
                    std::str::from_utf8(&s).unwrap(),
                    f32::from_bits(k),
                    2.0f32.powi(e),
                );
                for i in (3..10).rev() {
                    if s[i] == b'f' {
                        s[i] = b'0';
                    } else if s[i] == b'9' {
                        s[i] = b'a';
                        break;
                    } else {
                        s[i] += 1;
                        break;
                    }
                }
            }
            for i in (12..15).rev() {
                if s[i] == b'0' {
                    s[i] = b'9';
                } else {
                    s[i] -= 1;
                    break;
                }
            }
            for i in (3..10).rev() {
                s[i] = b'0';
            }
        }
    }

    // FIXME: this test is causing failures that are likely UB on various platforms
    #[cfg(all(target_arch = "x86_64", target_os = "linux"))]
    #[test]
    #[cfg(f128_enabled)]
    fn rounding() {
        let pi = std::f128::consts::PI;
        let s = format!("{}", Hexf(pi));

        for k in 0..=111 {
            let (bits, status) = parse_any(&s, 128 - k, 112 - k, Round::Nearest).unwrap();
            let scale = (1u128 << (112 - k - 1)) as f128;
            let expected = (pi * scale).round_ties_even() / scale;
            assert_eq!(bits << k, expected.to_bits(), "k = {k}, s = {s}");
            assert_eq!(expected != pi, status.inexact());
        }
    }
    #[test]
    fn rounding_extreme_underflow() {
        for k in 1..1000 {
            let s = format!("0x1p{}", -149 - k);
            let Ok((bits, status)) = parse_any(&s, 32, 23, Round::Nearest) else {
                unreachable!()
            };
            assert_eq!(bits, 0, "{s} should round to zero, got bits={bits}");
            assert!(
                status.underflow(),
                "should indicate underflow when parsing {s}"
            );
            assert!(status.inexact(), "should indicate inexact when parsing {s}");
        }
    }
    #[test]
    fn long_tail() {
        for k in 1..1000 {
            let s = format!("0x1.{}p0", "0".repeat(k));
            let Ok(bits) = parse_hex_exact(&s, 32, 23) else {
                panic!("parsing {s} failed")
            };
            assert_eq!(f32::from_bits(bits as u32), 1.0);

            let s = format!("0x1.{}1p0", "0".repeat(k));
            let Ok((bits, status)) = parse_any(&s, 32, 23, Round::Nearest) else {
                unreachable!()
            };
            if status.inexact() {
                assert!(1.0 == f32::from_bits(bits as u32));
            } else {
                assert!(1.0 < f32::from_bits(bits as u32));
            }
        }
    }
    // HACK(msrv): 1.63 rejects unknown width float literals at an AST level, so use a macro to
    // hide them from the AST.
    #[cfg(f16_enabled)]
    macro_rules! f16_tests {
        () => {
            #[test]
            fn test_f16() {
                let checks = [
                    ("0x.1234p+16", (0x1234 as f16).to_bits()),
                    ("0x1.234p+12", (0x1234 as f16).to_bits()),
                    ("0x12.34p+8", (0x1234 as f16).to_bits()),
                    ("0x123.4p+4", (0x1234 as f16).to_bits()),
                    ("0x1234p+0", (0x1234 as f16).to_bits()),
                    ("0x1234.p+0", (0x1234 as f16).to_bits()),
                    ("0x1234.0p+0", (0x1234 as f16).to_bits()),
                    ("0x1.ffcp+15", f16::MAX.to_bits()),
                    ("0x1.0p+1", 2.0f16.to_bits()),
                    ("0x1.0p+0", 1.0f16.to_bits()),
                    ("0x1.ffp+8", 0x5ffc),
                    ("+0x1.ffp+8", 0x5ffc),
                    ("0x1p+0", 0x3c00),
                    ("0x1.998p-4", 0x2e66),
                    ("0x1.9p+6", 0x5640),
                    ("0x0.0p0", 0.0f16.to_bits()),
                    ("-0x0.0p0", (-0.0f16).to_bits()),
                    ("0x1.0p0", 1.0f16.to_bits()),
                    ("0x1.998p-4", (0.1f16).to_bits()),
                    ("-0x1.998p-4", (-0.1f16).to_bits()),
                    ("0x0.123p-12", 0x0123),
                    ("0x1p-24", 0x0001),
                    ("nan", f16::NAN.to_bits()),
                    ("-nan", (-f16::NAN).to_bits()),
                    ("inf", f16::INFINITY.to_bits()),
                    ("-inf", f16::NEG_INFINITY.to_bits()),
                ];
                for (s, exp) in checks {
                    println!("parsing {s}");
                    assert!(rounding_properties(s).is_ok());
                    let act = hf16(s).to_bits();
                    assert_eq!(
                        act, exp,
                        "parsing {s}: {act:#06x} != {exp:#06x}\nact: {act:#018b}\nexp: {exp:#018b}"
                    );
                }
            }

            #[test]
            fn test_macros_f16() {
                assert_eq!(hf16!("0x1.ffp+8").to_bits(), 0x5ffc_u16);
            }
        };
    }

    #[cfg(f16_enabled)]
    f16_tests!();

    #[test]
    fn test_f32() {
        let checks = [
            ("0x.1234p+16", (0x1234 as f32).to_bits()),
            ("0x1.234p+12", (0x1234 as f32).to_bits()),
            ("0x12.34p+8", (0x1234 as f32).to_bits()),
            ("0x123.4p+4", (0x1234 as f32).to_bits()),
            ("0x1234p+0", (0x1234 as f32).to_bits()),
            ("0x1234.p+0", (0x1234 as f32).to_bits()),
            ("0x1234.0p+0", (0x1234 as f32).to_bits()),
            ("0x1.fffffep+127", f32::MAX.to_bits()),
            ("0x1.0p+1", 2.0f32.to_bits()),
            ("0x1.0p+0", 1.0f32.to_bits()),
            ("0x1.ffep+8", 0x43fff000),
            ("+0x1.ffep+8", 0x43fff000),
            ("0x1p+0", 0x3f800000),
            ("0x1.99999ap-4", 0x3dcccccd),
            ("0x1.9p+6", 0x42c80000),
            ("0x1.2d5ed2p+20", 0x4996af69),
            ("-0x1.348eb8p+10", 0xc49a475c),
            ("-0x1.33dcfep-33", 0xaf19ee7f),
            ("0x0.0p0", 0.0f32.to_bits()),
            ("-0x0.0p0", (-0.0f32).to_bits()),
            ("0x1.0p0", 1.0f32.to_bits()),
            ("0x1.99999ap-4", (0.1f32).to_bits()),
            ("-0x1.99999ap-4", (-0.1f32).to_bits()),
            ("0x1.111114p-127", 0x00444445),
            ("0x1.23456p-130", 0x00091a2b),
            ("0x1p-149", 0x00000001),
            ("nan", f32::NAN.to_bits()),
            ("-nan", (-f32::NAN).to_bits()),
            ("inf", f32::INFINITY.to_bits()),
            ("-inf", f32::NEG_INFINITY.to_bits()),
        ];
        for (s, exp) in checks {
            println!("parsing {s}");
            let act = hf32(s).to_bits();
            assert_eq!(
                act, exp,
                "parsing {s}: {act:#010x} != {exp:#010x}\nact: {act:#034b}\nexp: {exp:#034b}"
            );
        }
    }

    #[test]
    fn test_f64() {
        let checks = [
            ("0x.1234p+16", (0x1234 as f64).to_bits()),
            ("0x1.234p+12", (0x1234 as f64).to_bits()),
            ("0x12.34p+8", (0x1234 as f64).to_bits()),
            ("0x123.4p+4", (0x1234 as f64).to_bits()),
            ("0x1234p+0", (0x1234 as f64).to_bits()),
            ("0x1234.p+0", (0x1234 as f64).to_bits()),
            ("0x1234.0p+0", (0x1234 as f64).to_bits()),
            ("0x1.ffep+8", 0x407ffe0000000000),
            ("0x1p+0", 0x3ff0000000000000),
            ("0x1.999999999999ap-4", 0x3fb999999999999a),
            ("0x1.9p+6", 0x4059000000000000),
            ("0x1.2d5ed1fe1da7bp+20", 0x4132d5ed1fe1da7b),
            ("-0x1.348eb851eb852p+10", 0xc09348eb851eb852),
            ("-0x1.33dcfe54a3803p-33", 0xbde33dcfe54a3803),
            ("0x1.0p0", 1.0f64.to_bits()),
            ("0x0.0p0", 0.0f64.to_bits()),
            ("-0x0.0p0", (-0.0f64).to_bits()),
            ("0x1.999999999999ap-4", 0.1f64.to_bits()),
            ("0x1.999999999998ap-4", (0.1f64 - f64::EPSILON).to_bits()),
            ("-0x1.999999999999ap-4", (-0.1f64).to_bits()),
            ("-0x1.999999999998ap-4", (-0.1f64 + f64::EPSILON).to_bits()),
            ("0x0.8000000000001p-1022", 0x0008000000000001),
            ("0x0.123456789abcdp-1022", 0x000123456789abcd),
            ("0x0.0000000000002p-1022", 0x0000000000000002),
            ("nan", f64::NAN.to_bits()),
            ("-nan", (-f64::NAN).to_bits()),
            ("inf", f64::INFINITY.to_bits()),
            ("-inf", f64::NEG_INFINITY.to_bits()),
        ];
        for (s, exp) in checks {
            println!("parsing {s}");
            let act = hf64(s).to_bits();
            assert_eq!(
                act, exp,
                "parsing {s}: {act:#018x} != {exp:#018x}\nact: {act:#066b}\nexp: {exp:#066b}"
            );
        }
    }

    // HACK(msrv): 1.63 rejects unknown width float literals at an AST level, so use a macro to
    // hide them from the AST.
    #[cfg(f128_enabled)]
    macro_rules! f128_tests {
        () => {
            #[test]
            fn test_f128() {
                let checks = [
                    ("0x.1234p+16", (0x1234 as f128).to_bits()),
                    ("0x1.234p+12", (0x1234 as f128).to_bits()),
                    ("0x12.34p+8", (0x1234 as f128).to_bits()),
                    ("0x123.4p+4", (0x1234 as f128).to_bits()),
                    ("0x1234p+0", (0x1234 as f128).to_bits()),
                    ("0x1234.p+0", (0x1234 as f128).to_bits()),
                    ("0x1234.0p+0", (0x1234 as f128).to_bits()),
                    ("0x1.ffffffffffffffffffffffffffffp+16383", f128::MAX.to_bits()),
                    ("0x1.0p+1", 2.0f128.to_bits()),
                    ("0x1.0p+0", 1.0f128.to_bits()),
                    ("0x1.ffep+8", 0x4007ffe0000000000000000000000000),
                    ("+0x1.ffep+8", 0x4007ffe0000000000000000000000000),
                    ("0x1p+0", 0x3fff0000000000000000000000000000),
                    ("0x1.999999999999999999999999999ap-4", 0x3ffb999999999999999999999999999a),
                    ("0x1.9p+6", 0x40059000000000000000000000000000),
                    ("0x0.0p0", 0.0f128.to_bits()),
                    ("-0x0.0p0", (-0.0f128).to_bits()),
                    ("0x1.0p0", 1.0f128.to_bits()),
                    ("0x1.999999999999999999999999999ap-4", (0.1f128).to_bits()),
                    ("-0x1.999999999999999999999999999ap-4", (-0.1f128).to_bits()),
                    ("0x0.abcdef0123456789abcdef012345p-16382", 0x0000abcdef0123456789abcdef012345),
                    ("0x1p-16494", 0x00000000000000000000000000000001),
                    ("nan", f128::NAN.to_bits()),
                    ("-nan", (-f128::NAN).to_bits()),
                    ("inf", f128::INFINITY.to_bits()),
                    ("-inf", f128::NEG_INFINITY.to_bits()),
                ];
                for (s, exp) in checks {
                    println!("parsing {s}");
                    let act = hf128(s).to_bits();
                    assert_eq!(
                        act, exp,
                        "parsing {s}: {act:#034x} != {exp:#034x}\nact: {act:#0130b}\nexp: {exp:#0130b}"
                    );
                }
            }

            #[test]
            fn test_macros_f128() {
                assert_eq!(hf128!("0x1.ffep+8").to_bits(), 0x4007ffe0000000000000000000000000_u128);
            }
        }
    }

    #[cfg(f128_enabled)]
    f128_tests!();

    #[test]
    fn test_macros() {
        #[cfg(f16_enabled)]
        assert_eq!(hf16!("0x1.ffp+8").to_bits(), 0x5ffc_u16);
        assert_eq!(hf32!("0x1.ffep+8").to_bits(), 0x43fff000_u32);
        assert_eq!(hf64!("0x1.ffep+8").to_bits(), 0x407ffe0000000000_u64);
        #[cfg(f128_enabled)]
        assert_eq!(
            hf128!("0x1.ffep+8").to_bits(),
            0x4007ffe0000000000000000000000000_u128
        );
    }
}

#[cfg(test)]
// FIXME(ppc): something with `should_panic` tests cause a SIGILL with ppc64le
#[cfg(not(all(target_arch = "powerpc64", target_endian = "little")))]
mod tests_panicking {
    extern crate std;
    use super::*;

    // HACK(msrv): 1.63 rejects unknown width float literals at an AST level, so use a macro to
    // hide them from the AST.
    #[cfg(f16_enabled)]
    macro_rules! f16_tests {
        () => {
            #[test]
            fn test_f16_almost_extra_precision() {
                // Exact maximum precision allowed
                hf16("0x1.ffcp+0");
            }

            #[test]
            #[should_panic(expected = "the value is too precise")]
            fn test_f16_extra_precision() {
                // One bit more than the above.
                hf16("0x1.ffdp+0");
            }

            #[test]
            #[should_panic(expected = "the value is too huge")]
            fn test_f16_overflow() {
                // One bit more than the above.
                hf16("0x1p+16");
            }

            #[test]
            fn test_f16_tiniest() {
                let x = hf16("0x1.p-24");
                let y = hf16("0x0.001p-12");
                let z = hf16("0x0.8p-23");
                assert_eq!(x, y);
                assert_eq!(x, z);
            }

            #[test]
            #[should_panic(expected = "the value is too tiny")]
            fn test_f16_too_tiny() {
                hf16("0x1.p-25");
            }

            #[test]
            #[should_panic(expected = "the value is too tiny")]
            fn test_f16_also_too_tiny() {
                hf16("0x0.8p-24");
            }

            #[test]
            #[should_panic(expected = "the value is too tiny")]
            fn test_f16_again_too_tiny() {
                hf16("0x0.001p-13");
            }
        };
    }

    #[cfg(f16_enabled)]
    f16_tests!();

    #[test]
    fn test_f32_almost_extra_precision() {
        // Exact maximum precision allowed
        hf32("0x1.abcdeep+0");
    }

    #[test]
    #[should_panic]
    fn test_f32_extra_precision2() {
        // One bit more than the above.
        hf32("0x1.ffffffp+127");
    }

    #[test]
    #[should_panic(expected = "the value is too huge")]
    fn test_f32_overflow() {
        // One bit more than the above.
        hf32("0x1p+128");
    }

    #[test]
    #[should_panic(expected = "the value is too precise")]
    fn test_f32_extra_precision() {
        // One bit more than the above.
        hf32("0x1.abcdefp+0");
    }

    #[test]
    fn test_f32_tiniest() {
        let x = hf32("0x1.p-149");
        let y = hf32("0x0.0000000000000001p-85");
        let z = hf32("0x0.8p-148");
        assert_eq!(x, y);
        assert_eq!(x, z);
    }

    #[test]
    #[should_panic(expected = "the value is too tiny")]
    fn test_f32_too_tiny() {
        hf32("0x1.p-150");
    }

    #[test]
    #[should_panic(expected = "the value is too tiny")]
    fn test_f32_also_too_tiny() {
        hf32("0x0.8p-149");
    }

    #[test]
    #[should_panic(expected = "the value is too tiny")]
    fn test_f32_again_too_tiny() {
        hf32("0x0.0000000000000001p-86");
    }

    #[test]
    fn test_f64_almost_extra_precision() {
        // Exact maximum precision allowed
        hf64("0x1.abcdabcdabcdfp+0");
    }

    #[test]
    #[should_panic(expected = "the value is too precise")]
    fn test_f64_extra_precision() {
        // One bit more than the above.
        hf64("0x1.abcdabcdabcdf8p+0");
    }

    // HACK(msrv): 1.63 rejects unknown width float literals at an AST level, so use a macro to
    // hide them from the AST.
    #[cfg(f128_enabled)]
    macro_rules! f128_tests {
        () => {
            #[test]
            fn test_f128_almost_extra_precision() {
                // Exact maximum precision allowed
                hf128("0x1.ffffffffffffffffffffffffffffp+16383");
            }

            #[test]
            #[should_panic(expected = "the value is too precise")]
            fn test_f128_extra_precision() {
                // Just below the maximum finite.
                hf128("0x1.fffffffffffffffffffffffffffe8p+16383");
            }
            #[test]
            #[should_panic(expected = "the value is too huge")]
            fn test_f128_extra_precision_overflow() {
                // One bit more than the above. Should overflow.
                hf128("0x1.ffffffffffffffffffffffffffff8p+16383");
            }

            #[test]
            #[should_panic(expected = "the value is too huge")]
            fn test_f128_overflow() {
                // One bit more than the above.
                hf128("0x1p+16384");
            }

            #[test]
            fn test_f128_tiniest() {
                let x = hf128("0x1.p-16494");
                let y = hf128("0x0.0000000000000001p-16430");
                let z = hf128("0x0.8p-16493");
                assert_eq!(x, y);
                assert_eq!(x, z);
            }

            #[test]
            #[should_panic(expected = "the value is too tiny")]
            fn test_f128_too_tiny() {
                hf128("0x1.p-16495");
            }

            #[test]
            #[should_panic(expected = "the value is too tiny")]
            fn test_f128_again_too_tiny() {
                hf128("0x0.0000000000000001p-16431");
            }

            #[test]
            #[should_panic(expected = "the value is too tiny")]
            fn test_f128_also_too_tiny() {
                hf128("0x0.8p-16494");
            }
        };
    }

    #[cfg(f128_enabled)]
    f128_tests!();
}

#[cfg(test)]
mod print_tests {
    extern crate std;
    use std::string::ToString;

    use super::*;

    #[test]
    #[cfg(f16_enabled)]
    fn test_f16() {
        use std::format;
        // Exhaustively check that `f16` roundtrips.
        for x in 0..=u16::MAX {
            let f = f16::from_bits(x);
            let s = format!("{}", Hexf(f));
            let from_s = hf16(&s);

            if f.is_nan() && from_s.is_nan() {
                continue;
            }

            assert_eq!(
                f.to_bits(),
                from_s.to_bits(),
                "{f:?} formatted as {s} but parsed as {from_s:?}"
            );
        }
    }

    #[test]
    #[cfg(f16_enabled)]
    fn test_f16_to_f32() {
        use std::format;
        // Exhaustively check that these are equivalent for all `f16`:
        //  - `f16 -> f32`
        //  - `f16 -> str -> f32`
        //  - `f16 -> f32 -> str -> f32`
        //  - `f16 -> f32 -> str -> f16 -> f32`
        for x in 0..=u16::MAX {
            let f16 = f16::from_bits(x);
            let s16 = format!("{}", Hexf(f16));
            let f32 = f16 as f32;
            let s32 = format!("{}", Hexf(f32));

            let a = hf32(&s16);
            let b = hf32(&s32);
            let c = hf16(&s32);

            if f32.is_nan() && a.is_nan() && b.is_nan() && c.is_nan() {
                continue;
            }

            assert_eq!(
                f32.to_bits(),
                a.to_bits(),
                "{f16:?} : f16 formatted as {s16} which parsed as {a:?} : f16"
            );
            assert_eq!(
                f32.to_bits(),
                b.to_bits(),
                "{f32:?} : f32 formatted as {s32} which parsed as {b:?} : f32"
            );
            assert_eq!(
                f32.to_bits(),
                (c as f32).to_bits(),
                "{f32:?} : f32 formatted as {s32} which parsed as {c:?} : f16"
            );
        }
    }
    #[test]
    fn spot_checks() {
        assert_eq!(Hexf(f32::MAX).to_string(), "0x1.fffffep+127");
        assert_eq!(Hexf(f64::MAX).to_string(), "0x1.fffffffffffffp+1023");

        assert_eq!(Hexf(f32::MIN).to_string(), "-0x1.fffffep+127");
        assert_eq!(Hexf(f64::MIN).to_string(), "-0x1.fffffffffffffp+1023");

        assert_eq!(Hexf(f32::ZERO).to_string(), "0x0p+0");
        assert_eq!(Hexf(f64::ZERO).to_string(), "0x0p+0");

        assert_eq!(Hexf(f32::NEG_ZERO).to_string(), "-0x0p+0");
        assert_eq!(Hexf(f64::NEG_ZERO).to_string(), "-0x0p+0");

        assert_eq!(Hexf(f32::NAN).to_string(), "NaN");
        assert_eq!(Hexf(f64::NAN).to_string(), "NaN");

        assert_eq!(Hexf(f32::INFINITY).to_string(), "inf");
        assert_eq!(Hexf(f64::INFINITY).to_string(), "inf");

        assert_eq!(Hexf(f32::NEG_INFINITY).to_string(), "-inf");
        assert_eq!(Hexf(f64::NEG_INFINITY).to_string(), "-inf");

        #[cfg(f16_enabled)]
        {
            assert_eq!(Hexf(f16::MAX).to_string(), "0x1.ffcp+15");
            assert_eq!(Hexf(f16::MIN).to_string(), "-0x1.ffcp+15");
            assert_eq!(Hexf(f16::ZERO).to_string(), "0x0p+0");
            assert_eq!(Hexf(f16::NEG_ZERO).to_string(), "-0x0p+0");
            assert_eq!(Hexf(f16::NAN).to_string(), "NaN");
            assert_eq!(Hexf(f16::INFINITY).to_string(), "inf");
            assert_eq!(Hexf(f16::NEG_INFINITY).to_string(), "-inf");
        }

        #[cfg(f128_enabled)]
        {
            assert_eq!(
                Hexf(f128::MAX).to_string(),
                "0x1.ffffffffffffffffffffffffffffp+16383"
            );
            assert_eq!(
                Hexf(f128::MIN).to_string(),
                "-0x1.ffffffffffffffffffffffffffffp+16383"
            );
            assert_eq!(Hexf(f128::ZERO).to_string(), "0x0p+0");
            assert_eq!(Hexf(f128::NEG_ZERO).to_string(), "-0x0p+0");
            assert_eq!(Hexf(f128::NAN).to_string(), "NaN");
            assert_eq!(Hexf(f128::INFINITY).to_string(), "inf");
            assert_eq!(Hexf(f128::NEG_INFINITY).to_string(), "-inf");
        }
    }
}
