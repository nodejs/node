//! Efficient and customizable data-encoding functions like base64, base32, and hex
//!
//! This [crate] provides little-endian ASCII base-conversion encodings for
//! bases of size 2, 4, 8, 16, 32, and 64. It supports:
//!
//! - [padding] for streaming
//! - canonical encodings (e.g. [trailing bits] are checked)
//! - in-place [encoding] and [decoding] functions
//! - partial [decoding] functions (e.g. for error recovery)
//! - character [translation] (e.g. for case-insensitivity)
//! - most and least significant [bit-order]
//! - [ignoring] characters when decoding (e.g. for skipping newlines)
//! - [wrapping] the output when encoding
//! - no-std environments with `default-features = false, features = ["alloc"]`
//! - no-alloc environments with `default-features = false`
//!
//! You may use the [binary] or the [website] to play around.
//!
//! # Examples
//!
//! This crate provides predefined encodings as [constants]. These constants are of type
//! [`Encoding`]. This type provides encoding and decoding functions with in-place or allocating
//! variants. Here is an example using the allocating encoding function of [`BASE64`]:
//!
//! ```rust
//! use data_encoding::BASE64;
//! assert_eq!(BASE64.encode(b"Hello world"), "SGVsbG8gd29ybGQ=");
//! ```
//!
//! Here is an example using the in-place decoding function of [`BASE32`]:
//!
//! ```rust
//! use data_encoding::BASE32;
//! let input = b"JBSWY3DPEB3W64TMMQ======";
//! let mut output = vec![0; BASE32.decode_len(input.len()).unwrap()];
//! let len = BASE32.decode_mut(input, &mut output).unwrap();
//! assert_eq!(&output[0 .. len], b"Hello world");
//! ```
//!
//! You are not limited to the predefined encodings. You may define your own encodings (with the
//! same correctness and performance properties as the predefined ones) using the [`Specification`]
//! type:
//!
//! ```rust
//! use data_encoding::Specification;
//! let hex = {
//!     let mut spec = Specification::new();
//!     spec.symbols.push_str("0123456789abcdef");
//!     spec.encoding().unwrap()
//! };
//! assert_eq!(hex.encode(b"hello"), "68656c6c6f");
//! ```
//!
//! You may use the [macro] library to define a compile-time custom encoding:
//!
//! ```rust,ignore
//! use data_encoding::Encoding;
//! use data_encoding_macro::new_encoding;
//! const HEX: Encoding = new_encoding!{
//!     symbols: "0123456789abcdef",
//!     translate_from: "ABCDEF",
//!     translate_to: "abcdef",
//! };
//! const BASE64: Encoding = new_encoding!{
//!     symbols: "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
//!     padding: '=',
//! };
//! ```
//!
//! # Properties
//!
//! The [`HEXUPPER`], [`BASE32`], [`BASE32HEX`], [`BASE64`], and [`BASE64URL`] predefined encodings
//! conform to [RFC4648].
//!
//! In general, the encoding and decoding functions satisfy the following properties:
//!
//! - They are deterministic: their output only depends on their input
//! - They have no side-effects: they do not modify any hidden mutable state
//! - They are correct: encoding followed by decoding gives the initial data
//! - They are canonical (unless [`is_canonical`] returns false): decoding followed by encoding
//!   gives the initial data
//!
//! This last property is usually not satisfied by base64 implementations. This is a matter of
//! choice and this crate has made the choice to let the user choose. Support for canonical encoding
//! as described by the [RFC][canonical] is provided. But it is also possible to disable checking
//! trailing bits, to add characters translation, to decode concatenated padded inputs, and to
//! ignore some characters. Note that non-canonical encodings may be an attack vector as described
//! in [Base64 Malleability in Practice](https://eprint.iacr.org/2022/361.pdf).
//!
//! Since the RFC specifies the encoding function on all inputs and the decoding function on all
//! possible encoded outputs, the differences between implementations come from the decoding
//! function which may be more or less permissive. In this crate, the decoding function of canonical
//! encodings rejects all inputs that are not a possible output of the encoding function. Here are
//! some concrete examples of decoding differences between this crate, the `base64` crate, and the
//! `base64` GNU program:
//!
//! | Input      | `data-encoding` | `base64`  | GNU `base64`  |
//! | ---------- | --------------- | --------- | ------------- |
//! | `AAB=`     | `Trailing(2)`   | `Last(2)` | `\x00\x00`    |
//! | `AA\nB=`   | `Length(4)`     | `Byte(2)` | `\x00\x00`    |
//! | `AAB`      | `Length(0)`     | `Padding` | Invalid input |
//! | `AAA`      | `Length(0)`     | `Padding` | Invalid input |
//! | `A\rA\nB=` | `Length(4)`     | `Byte(1)` | Invalid input |
//! | `-_\r\n`   | `Symbol(0)`     | `Byte(0)` | Invalid input |
//! | `AA==AA==` | `[0, 0]`        | `Byte(2)` | `\x00\x00`    |
//!
//! We can summarize these discrepancies as follows:
//!
//! | Discrepancy                | `data-encoding` | `base64` | GNU `base64` |
//! | -------------------------- | --------------- | -------- | ------------ |
//! | Check trailing bits        | Yes             | Yes      | No           |
//! | Ignored characters         | None            | None     | `\n`         |
//! | Translated characters      | None            | None     | None         |
//! | Check padding              | Yes             | No       | Yes          |
//! | Support concatenated input | Yes             | No       | Yes          |
//!
//! This crate permits to disable checking trailing bits. It permits to ignore some characters. It
//! permits to translate characters. It permits to use unpadded encodings. However, for padded
//! encodings, support for concatenated inputs cannot be disabled. This is simply because it doesn't
//! make sense to use padding if it is not to support concatenated inputs.
//!
//! [RFC4648]: https://tools.ietf.org/html/rfc4648
//! [`BASE32HEX`]: constant.BASE32HEX.html
//! [`BASE32`]: constant.BASE32.html
//! [`BASE64URL`]: constant.BASE64URL.html
//! [`BASE64`]: constant.BASE64.html
//! [`Encoding`]: struct.Encoding.html
//! [`HEXUPPER`]: constant.HEXUPPER.html
//! [`Specification`]: struct.Specification.html
//! [`is_canonical`]: struct.Encoding.html#method.is_canonical
//! [binary]: https://crates.io/crates/data-encoding-bin
//! [bit-order]: struct.Specification.html#structfield.bit_order
//! [canonical]: https://tools.ietf.org/html/rfc4648#section-3.5
//! [constants]: index.html#constants
//! [crate]: https://crates.io/crates/data-encoding
//! [decoding]: struct.Encoding.html#method.decode_mut
//! [encoding]: struct.Encoding.html#method.encode_mut
//! [ignoring]: struct.Specification.html#structfield.ignore
//! [macro]: https://crates.io/crates/data-encoding-macro
//! [padding]: struct.Specification.html#structfield.padding
//! [trailing bits]: struct.Specification.html#structfield.check_trailing_bits
//! [translation]: struct.Specification.html#structfield.translate
//! [website]: https://data-encoding.rs
//! [wrapping]: struct.Specification.html#structfield.wrap

#![no_std]
#![cfg_attr(docsrs, feature(doc_cfg))]

#[cfg(feature = "alloc")]
extern crate alloc;
#[cfg(feature = "std")]
extern crate std;

#[cfg(feature = "alloc")]
use alloc::borrow::{Cow, ToOwned};
#[cfg(feature = "alloc")]
use alloc::string::String;
#[cfg(feature = "alloc")]
use alloc::vec;
#[cfg(feature = "alloc")]
use alloc::vec::Vec;
use core::convert::TryInto;
use core::debug_assert as safety_assert;

macro_rules! check {
    ($e: expr, $c: expr) => {
        if !$c {
            return Err($e);
        }
    };
}

trait Static<T: Copy>: Copy {
    fn val(self) -> T;
}

macro_rules! define {
    ($name: ident: $type: ty = $val: expr) => {
        #[derive(Copy, Clone)]
        struct $name;
        impl Static<$type> for $name {
            fn val(self) -> $type {
                $val
            }
        }
    };
}

define!(Bf: bool = false);
define!(Bt: bool = true);
define!(N1: usize = 1);
define!(N2: usize = 2);
define!(N3: usize = 3);
define!(N4: usize = 4);
define!(N5: usize = 5);
define!(N6: usize = 6);

#[derive(Copy, Clone)]
struct On;

impl<T: Copy> Static<Option<T>> for On {
    fn val(self) -> Option<T> {
        None
    }
}

#[derive(Copy, Clone)]
struct Os<T>(T);

impl<T: Copy> Static<Option<T>> for Os<T> {
    fn val(self) -> Option<T> {
        Some(self.0)
    }
}

macro_rules! dispatch {
    (let $var: ident: bool = $val: expr; $($body: tt)*) => {
        if $val {
            let $var = Bt; dispatch!($($body)*)
        } else {
            let $var = Bf; dispatch!($($body)*)
        }
    };
    (let $var: ident: usize = $val: expr; $($body: tt)*) => {
        match $val {
            1 => { let $var = N1; dispatch!($($body)*) },
            2 => { let $var = N2; dispatch!($($body)*) },
            3 => { let $var = N3; dispatch!($($body)*) },
            4 => { let $var = N4; dispatch!($($body)*) },
            5 => { let $var = N5; dispatch!($($body)*) },
            6 => { let $var = N6; dispatch!($($body)*) },
            _ => panic!(),
        }
    };
    (let $var: ident: Option<$type: ty> = $val: expr; $($body: tt)*) => {
        match $val {
            None => { let $var = On; dispatch!($($body)*) },
            Some(x) => { let $var = Os(x); dispatch!($($body)*) },
        }
    };
    ($body: expr) => { $body };
}

fn chunk_unchecked<T>(x: &[T], n: usize, i: usize) -> &[T] {
    safety_assert!((i + 1) * n <= x.len());
    // SAFETY: Ensured by correctness requirements (and asserted above).
    unsafe { core::slice::from_raw_parts(x.as_ptr().add(n * i), n) }
}

fn chunk_mut_unchecked<T>(x: &mut [T], n: usize, i: usize) -> &mut [T] {
    safety_assert!((i + 1) * n <= x.len());
    // SAFETY: Ensured by correctness requirements (and asserted above).
    unsafe { core::slice::from_raw_parts_mut(x.as_mut_ptr().add(n * i), n) }
}

fn div_ceil(x: usize, m: usize) -> usize {
    (x + m - 1) / m
}

fn floor(x: usize, m: usize) -> usize {
    x / m * m
}

#[inline]
fn vectorize<F: FnMut(usize)>(n: usize, bs: usize, mut f: F) {
    for k in 0 .. n / bs {
        for i in k * bs .. (k + 1) * bs {
            f(i);
        }
    }
    for i in floor(n, bs) .. n {
        f(i);
    }
}

/// Decoding error kind
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum DecodeKind {
    /// Invalid length
    Length,

    /// Invalid symbol
    Symbol,

    /// Non-zero trailing bits
    Trailing,

    /// Invalid padding length
    Padding,
}

impl core::fmt::Display for DecodeKind {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        let description = match self {
            DecodeKind::Length => "invalid length",
            DecodeKind::Symbol => "invalid symbol",
            DecodeKind::Trailing => "non-zero trailing bits",
            DecodeKind::Padding => "invalid padding length",
        };
        write!(f, "{}", description)
    }
}

/// Decoding error
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub struct DecodeError {
    /// Error position
    ///
    /// This position is always a valid input position and represents the first encountered error.
    pub position: usize,

    /// Error kind
    pub kind: DecodeKind,
}

#[cfg(feature = "std")]
impl std::error::Error for DecodeError {}

impl core::fmt::Display for DecodeError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "{} at {}", self.kind, self.position)
    }
}

/// Decoding error with partial result
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub struct DecodePartial {
    /// Number of bytes read from input
    ///
    /// This number does not exceed the error position: `read <= error.position`.
    pub read: usize,

    /// Number of bytes written to output
    ///
    /// This number does not exceed the decoded length: `written <= decode_len(read)`.
    pub written: usize,

    /// Decoding error
    pub error: DecodeError,
}

const INVALID: u8 = 128;
const IGNORE: u8 = 129;
const PADDING: u8 = 130;

fn order(msb: bool, n: usize, i: usize) -> usize {
    if msb {
        n - 1 - i
    } else {
        i
    }
}

#[inline]
fn enc(bit: usize) -> usize {
    match bit {
        1 | 2 | 4 => 1,
        3 | 6 => 3,
        5 => 5,
        _ => unreachable!(),
    }
}

#[inline]
fn dec(bit: usize) -> usize {
    enc(bit) * 8 / bit
}

fn encode_len<B: Static<usize>>(bit: B, len: usize) -> usize {
    div_ceil(8 * len, bit.val())
}

fn encode_block<B: Static<usize>, M: Static<bool>>(
    bit: B, msb: M, symbols: &[u8; 256], input: &[u8], output: &mut [u8],
) {
    debug_assert!(input.len() <= enc(bit.val()));
    debug_assert_eq!(output.len(), encode_len(bit, input.len()));
    let bit = bit.val();
    let msb = msb.val();
    let mut x = 0u64;
    for (i, input) in input.iter().enumerate() {
        x |= u64::from(*input) << (8 * order(msb, enc(bit), i));
    }
    for (i, output) in output.iter_mut().enumerate() {
        let y = x >> (bit * order(msb, dec(bit), i));
        *output = symbols[(y & 0xff) as usize];
    }
}

fn encode_mut<B: Static<usize>, M: Static<bool>>(
    bit: B, msb: M, symbols: &[u8; 256], input: &[u8], output: &mut [u8],
) {
    debug_assert_eq!(output.len(), encode_len(bit, input.len()));
    let enc = enc(bit.val());
    let dec = dec(bit.val());
    let n = input.len() / enc;
    let bs = match bit.val() {
        5 => 2,
        6 => 4,
        _ => 1,
    };
    vectorize(n, bs, |i| {
        let input = chunk_unchecked(input, enc, i);
        let output = chunk_mut_unchecked(output, dec, i);
        encode_block(bit, msb, symbols, input, output);
    });
    encode_block(bit, msb, symbols, &input[enc * n ..], &mut output[dec * n ..]);
}

// Fails if an input character does not translate to a symbol. The error is the
// lowest index of such character. The output is not written to.
fn decode_block<B: Static<usize>, M: Static<bool>>(
    bit: B, msb: M, values: &[u8; 256], input: &[u8], output: &mut [u8],
) -> Result<(), usize> {
    debug_assert!(output.len() <= enc(bit.val()));
    debug_assert_eq!(input.len(), encode_len(bit, output.len()));
    let bit = bit.val();
    let msb = msb.val();
    let mut x = 0u64;
    for j in 0 .. input.len() {
        let y = values[input[j] as usize];
        check!(j, y < 1 << bit);
        x |= u64::from(y) << (bit * order(msb, dec(bit), j));
    }
    for (j, output) in output.iter_mut().enumerate() {
        *output = ((x >> (8 * order(msb, enc(bit), j))) & 0xff) as u8;
    }
    Ok(())
}

// Fails if an input character does not translate to a symbol. The error `pos`
// is the lowest index of such character. The output is valid up to `pos / dec *
// enc` excluded.
fn decode_mut<B: Static<usize>, M: Static<bool>>(
    bit: B, msb: M, values: &[u8; 256], input: &[u8], output: &mut [u8],
) -> Result<(), usize> {
    debug_assert_eq!(input.len(), encode_len(bit, output.len()));
    let enc = enc(bit.val());
    let dec = dec(bit.val());
    let n = input.len() / dec;
    for i in 0 .. n {
        let input = chunk_unchecked(input, dec, i);
        let output = chunk_mut_unchecked(output, enc, i);
        decode_block(bit, msb, values, input, output).map_err(|e| dec * i + e)?;
    }
    decode_block(bit, msb, values, &input[dec * n ..], &mut output[enc * n ..])
        .map_err(|e| dec * n + e)
}

// Fails if there are non-zero trailing bits.
fn check_trail<B: Static<usize>, M: Static<bool>>(
    bit: B, msb: M, ctb: bool, values: &[u8; 256], input: &[u8],
) -> Result<(), ()> {
    if 8 % bit.val() == 0 || !ctb {
        return Ok(());
    }
    let trail = bit.val() * input.len() % 8;
    if trail == 0 {
        return Ok(());
    }
    let mut mask = (1 << trail) - 1;
    if !msb.val() {
        mask <<= bit.val() - trail;
    }
    check!((), values[input[input.len() - 1] as usize] & mask == 0);
    Ok(())
}

// Fails if the padding length is invalid. The error is the index of the first
// padding character.
fn check_pad<B: Static<usize>>(bit: B, values: &[u8; 256], input: &[u8]) -> Result<usize, usize> {
    let bit = bit.val();
    debug_assert_eq!(input.len(), dec(bit));
    let is_pad = |x: &&u8| values[**x as usize] == PADDING;
    let count = input.iter().rev().take_while(is_pad).count();
    let len = input.len() - count;
    check!(len, len > 0 && bit * len % 8 < bit);
    Ok(len)
}

fn encode_base_len<B: Static<usize>>(bit: B, len: usize) -> usize {
    encode_len(bit, len)
}

fn encode_base<B: Static<usize>, M: Static<bool>>(
    bit: B, msb: M, symbols: &[u8; 256], input: &[u8], output: &mut [u8],
) {
    debug_assert_eq!(output.len(), encode_base_len(bit, input.len()));
    encode_mut(bit, msb, symbols, input, output);
}

fn encode_pad_len<B: Static<usize>, P: Static<Option<u8>>>(bit: B, pad: P, len: usize) -> usize {
    match pad.val() {
        None => encode_base_len(bit, len),
        Some(_) => div_ceil(len, enc(bit.val())) * dec(bit.val()),
    }
}

fn encode_pad<B: Static<usize>, M: Static<bool>, P: Static<Option<u8>>>(
    bit: B, msb: M, symbols: &[u8; 256], spad: P, input: &[u8], output: &mut [u8],
) {
    let pad = match spad.val() {
        None => return encode_base(bit, msb, symbols, input, output),
        Some(pad) => pad,
    };
    debug_assert_eq!(output.len(), encode_pad_len(bit, spad, input.len()));
    let olen = encode_base_len(bit, input.len());
    encode_base(bit, msb, symbols, input, &mut output[.. olen]);
    for output in output.iter_mut().skip(olen) {
        *output = pad;
    }
}

fn encode_wrap_len<
    'a,
    B: Static<usize>,
    P: Static<Option<u8>>,
    W: Static<Option<(usize, &'a [u8])>>,
>(
    bit: B, pad: P, wrap: W, ilen: usize,
) -> usize {
    let olen = encode_pad_len(bit, pad, ilen);
    match wrap.val() {
        None => olen,
        Some((col, end)) => olen + end.len() * div_ceil(olen, col),
    }
}

fn encode_wrap_mut<
    'a,
    B: Static<usize>,
    M: Static<bool>,
    P: Static<Option<u8>>,
    W: Static<Option<(usize, &'a [u8])>>,
>(
    bit: B, msb: M, symbols: &[u8; 256], pad: P, wrap: W, input: &[u8], output: &mut [u8],
) {
    let (col, end) = match wrap.val() {
        None => return encode_pad(bit, msb, symbols, pad, input, output),
        Some((col, end)) => (col, end),
    };
    debug_assert_eq!(output.len(), encode_wrap_len(bit, pad, wrap, input.len()));
    debug_assert_eq!(col % dec(bit.val()), 0);
    let col = col / dec(bit.val());
    let enc = col * enc(bit.val());
    let dec = col * dec(bit.val()) + end.len();
    let olen = dec - end.len();
    let n = input.len() / enc;
    for i in 0 .. n {
        let input = chunk_unchecked(input, enc, i);
        let output = chunk_mut_unchecked(output, dec, i);
        encode_base(bit, msb, symbols, input, &mut output[.. olen]);
        output[olen ..].copy_from_slice(end);
    }
    if input.len() > enc * n {
        let olen = dec * n + encode_pad_len(bit, pad, input.len() - enc * n);
        encode_pad(bit, msb, symbols, pad, &input[enc * n ..], &mut output[dec * n .. olen]);
        output[olen ..].copy_from_slice(end);
    }
}

// Returns the longest valid input length and associated output length.
fn decode_wrap_len<B: Static<usize>, P: Static<bool>>(
    bit: B, pad: P, len: usize,
) -> (usize, usize) {
    let bit = bit.val();
    if pad.val() {
        (floor(len, dec(bit)), len / dec(bit) * enc(bit))
    } else {
        let trail = bit * len % 8;
        (len - trail / bit, bit * len / 8)
    }
}

// Fails with Length if length is invalid. The error is the largest valid
// length.
fn decode_pad_len<B: Static<usize>, P: Static<bool>>(
    bit: B, pad: P, len: usize,
) -> Result<usize, DecodeError> {
    let (ilen, olen) = decode_wrap_len(bit, pad, len);
    check!(DecodeError { position: ilen, kind: DecodeKind::Length }, ilen == len);
    Ok(olen)
}

// Fails with Length if length is invalid. The error is the largest valid
// length.
fn decode_base_len<B: Static<usize>>(bit: B, len: usize) -> Result<usize, DecodeError> {
    decode_pad_len(bit, Bf, len)
}

// Fails with Symbol if an input character does not translate to a symbol. The
// error is the lowest index of such character.
// Fails with Trailing if there are non-zero trailing bits.
fn decode_base_mut<B: Static<usize>, M: Static<bool>>(
    bit: B, msb: M, ctb: bool, values: &[u8; 256], input: &[u8], output: &mut [u8],
) -> Result<usize, DecodePartial> {
    debug_assert_eq!(Ok(output.len()), decode_base_len(bit, input.len()));
    let fail = |pos, kind| DecodePartial {
        read: pos / dec(bit.val()) * dec(bit.val()),
        written: pos / dec(bit.val()) * enc(bit.val()),
        error: DecodeError { position: pos, kind },
    };
    decode_mut(bit, msb, values, input, output).map_err(|pos| fail(pos, DecodeKind::Symbol))?;
    check_trail(bit, msb, ctb, values, input)
        .map_err(|()| fail(input.len() - 1, DecodeKind::Trailing))?;
    Ok(output.len())
}

// Fails with Symbol if an input character does not translate to a symbol. The
// error is the lowest index of such character.
// Fails with Padding if some padding length is invalid. The error is the index
// of the first padding character of the invalid padding.
// Fails with Trailing if there are non-zero trailing bits.
fn decode_pad_mut<B: Static<usize>, M: Static<bool>, P: Static<bool>>(
    bit: B, msb: M, ctb: bool, values: &[u8; 256], pad: P, input: &[u8], output: &mut [u8],
) -> Result<usize, DecodePartial> {
    if !pad.val() {
        return decode_base_mut(bit, msb, ctb, values, input, output);
    }
    debug_assert_eq!(Ok(output.len()), decode_pad_len(bit, pad, input.len()));
    let enc = enc(bit.val());
    let dec = dec(bit.val());
    let mut inpos = 0;
    let mut outpos = 0;
    let mut outend = output.len();
    while inpos < input.len() {
        match decode_base_mut(
            bit,
            msb,
            ctb,
            values,
            &input[inpos ..],
            &mut output[outpos .. outend],
        ) {
            Ok(written) => {
                if cfg!(debug_assertions) {
                    inpos = input.len();
                }
                outpos += written;
                break;
            }
            Err(partial) => {
                inpos += partial.read;
                outpos += partial.written;
            }
        }
        let inlen =
            check_pad(bit, values, &input[inpos .. inpos + dec]).map_err(|pos| DecodePartial {
                read: inpos,
                written: outpos,
                error: DecodeError { position: inpos + pos, kind: DecodeKind::Padding },
            })?;
        let outlen = decode_base_len(bit, inlen).unwrap();
        let written = decode_base_mut(
            bit,
            msb,
            ctb,
            values,
            &input[inpos .. inpos + inlen],
            &mut output[outpos .. outpos + outlen],
        )
        .map_err(|partial| {
            debug_assert_eq!(partial.read, 0);
            debug_assert_eq!(partial.written, 0);
            DecodePartial {
                read: inpos,
                written: outpos,
                error: DecodeError {
                    position: inpos + partial.error.position,
                    kind: partial.error.kind,
                },
            }
        })?;
        debug_assert_eq!(written, outlen);
        inpos += dec;
        outpos += outlen;
        outend -= enc - outlen;
    }
    debug_assert_eq!(inpos, input.len());
    debug_assert_eq!(outpos, outend);
    Ok(outend)
}

fn skip_ignore(values: &[u8; 256], input: &[u8], mut inpos: usize) -> usize {
    while inpos < input.len() && values[input[inpos] as usize] == IGNORE {
        inpos += 1;
    }
    inpos
}

// Returns next input and output position.
// Fails with Symbol if an input character does not translate to a symbol. The
// error is the lowest index of such character.
// Fails with Padding if some padding length is invalid. The error is the index
// of the first padding character of the invalid padding.
// Fails with Trailing if there are non-zero trailing bits.
fn decode_wrap_block<B: Static<usize>, M: Static<bool>, P: Static<bool>>(
    bit: B, msb: M, ctb: bool, values: &[u8; 256], pad: P, input: &[u8], output: &mut [u8],
) -> Result<(usize, usize), DecodeError> {
    let dec = dec(bit.val());
    let mut buf = [0u8; 8];
    let mut shift = [0usize; 8];
    let mut bufpos = 0;
    let mut inpos = 0;
    while bufpos < dec {
        inpos = skip_ignore(values, input, inpos);
        if inpos == input.len() {
            break;
        }
        shift[bufpos] = inpos;
        buf[bufpos] = input[inpos];
        bufpos += 1;
        inpos += 1;
    }
    let olen = decode_pad_len(bit, pad, bufpos).map_err(|mut e| {
        e.position = shift[e.position];
        e
    })?;
    let written = decode_pad_mut(bit, msb, ctb, values, pad, &buf[.. bufpos], &mut output[.. olen])
        .map_err(|partial| {
            debug_assert_eq!(partial.read, 0);
            debug_assert_eq!(partial.written, 0);
            DecodeError { position: shift[partial.error.position], kind: partial.error.kind }
        })?;
    Ok((inpos, written))
}

// Fails with Symbol if an input character does not translate to a symbol. The
// error is the lowest index of such character.
// Fails with Padding if some padding length is invalid. The error is the index
// of the first padding character of the invalid padding.
// Fails with Trailing if there are non-zero trailing bits.
// Fails with Length if input length (without ignored characters) is invalid.
#[allow(clippy::too_many_arguments)]
fn decode_wrap_mut<B: Static<usize>, M: Static<bool>, P: Static<bool>, I: Static<bool>>(
    bit: B, msb: M, ctb: bool, values: &[u8; 256], pad: P, has_ignore: I, input: &[u8],
    output: &mut [u8],
) -> Result<usize, DecodePartial> {
    if !has_ignore.val() {
        return decode_pad_mut(bit, msb, ctb, values, pad, input, output);
    }
    debug_assert_eq!(output.len(), decode_wrap_len(bit, pad, input.len()).1);
    let mut inpos = 0;
    let mut outpos = 0;
    while inpos < input.len() {
        let (inlen, outlen) = decode_wrap_len(bit, pad, input.len() - inpos);
        match decode_pad_mut(
            bit,
            msb,
            ctb,
            values,
            pad,
            &input[inpos .. inpos + inlen],
            &mut output[outpos .. outpos + outlen],
        ) {
            Ok(written) => {
                inpos += inlen;
                outpos += written;
                break;
            }
            Err(partial) => {
                inpos += partial.read;
                outpos += partial.written;
            }
        }
        let (ipos, opos) =
            decode_wrap_block(bit, msb, ctb, values, pad, &input[inpos ..], &mut output[outpos ..])
                .map_err(|mut error| {
                    error.position += inpos;
                    DecodePartial { read: inpos, written: outpos, error }
                })?;
        inpos += ipos;
        outpos += opos;
    }
    let inpos = skip_ignore(values, input, inpos);
    if inpos == input.len() {
        Ok(outpos)
    } else {
        Err(DecodePartial {
            read: inpos,
            written: outpos,
            error: DecodeError { position: inpos, kind: DecodeKind::Length },
        })
    }
}

/// Order in which bits are read from a byte
///
/// The base-conversion encoding is always little-endian. This means that the least significant
/// **byte** is always first. However, we can still choose whether, within a byte, this is the most
/// significant or the least significant **bit** that is first. If the terminology is confusing,
/// testing on an asymmetrical example should be enough to choose the correct value.
///
/// # Examples
///
/// In the following example, we can see that a base with the `MostSignificantFirst` bit-order has
/// the most significant bit first in the encoded output. In particular, the output is in the same
/// order as the bits in the byte. The opposite happens with the `LeastSignificantFirst` bit-order.
/// The least significant bit is first and the output is in the reverse order.
///
/// ```rust
/// use data_encoding::{BitOrder, Specification};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("01");
/// spec.bit_order = BitOrder::MostSignificantFirst;  // default
/// let msb = spec.encoding().unwrap();
/// spec.bit_order = BitOrder::LeastSignificantFirst;
/// let lsb = spec.encoding().unwrap();
/// assert_eq!(msb.encode(&[0b01010011]), "01010011");
/// assert_eq!(lsb.encode(&[0b01010011]), "11001010");
/// ```
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[cfg(feature = "alloc")]
pub enum BitOrder {
    /// Most significant bit first
    ///
    /// This is the most common and most intuitive bit-order. In particular, this is the bit-order
    /// used by [RFC4648] and thus the usual hexadecimal, base64, base32, base64url, and base32hex
    /// encodings. This is the default bit-order when [specifying](struct.Specification.html) a
    /// base.
    ///
    /// [RFC4648]: https://tools.ietf.org/html/rfc4648
    MostSignificantFirst,

    /// Least significant bit first
    ///
    /// # Examples
    ///
    /// DNSCurve [base32] uses least significant bit first:
    ///
    /// ```rust
    /// use data_encoding::BASE32_DNSCURVE;
    /// assert_eq!(BASE32_DNSCURVE.encode(&[0x64, 0x88]), "4321");
    /// assert_eq!(BASE32_DNSCURVE.decode(b"4321").unwrap(), vec![0x64, 0x88]);
    /// ```
    ///
    /// [base32]: constant.BASE32_DNSCURVE.html
    LeastSignificantFirst,
}
#[cfg(feature = "alloc")]
use crate::BitOrder::*;

#[doc(hidden)]
#[cfg(feature = "alloc")]
pub type InternalEncoding = Cow<'static, [u8]>;

#[doc(hidden)]
#[cfg(not(feature = "alloc"))]
pub type InternalEncoding = &'static [u8];

/// Base-conversion encoding
///
/// See [Specification](struct.Specification.html) for technical details or how to define a new one.
// Required fields:
//   0 - 256 (256) symbols
// 256 - 512 (256) values
// 512 - 513 (  1) padding
// 513 - 514 (  1) reserved(3),ctb(1),msb(1),bit(3)
// Optional fields:
// 514 - 515 (  1) width
// 515 -   * (  N) separator
// Invariants:
// - symbols is 2^bit unique characters repeated 2^(8-bit) times
// - values[128 ..] are INVALID
// - values[0 .. 128] are either INVALID, IGNORE, PADDING, or < 2^bit
// - padding is either < 128 or INVALID
// - values[padding] is PADDING if padding < 128
// - values and symbols are inverse
// - ctb is true if 8 % bit == 0
// - width is present if there is x such that values[x] is IGNORE
// - width % dec(bit) == 0
// - for all x in separator values[x] is IGNORE
#[derive(Debug, Clone, PartialEq, Eq)]
#[repr(transparent)]
pub struct Encoding(#[doc(hidden)] pub InternalEncoding);

/// How to translate characters when decoding
///
/// The order matters. The first character of the `from` field is translated to the first character
/// of the `to` field. The second to the second. Etc.
///
/// See [Specification](struct.Specification.html) for more information.
#[derive(Debug, Clone)]
#[cfg(feature = "alloc")]
pub struct Translate {
    /// Characters to translate from
    pub from: String,

    /// Characters to translate to
    pub to: String,
}

/// How to wrap the output when encoding
///
/// See [Specification](struct.Specification.html) for more information.
#[derive(Debug, Clone)]
#[cfg(feature = "alloc")]
pub struct Wrap {
    /// Wrapping width
    ///
    /// Must be a multiple of:
    ///
    /// - 8 for a bit-width of 1 (binary), 3 (octal), and 5 (base32)
    /// - 4 for a bit-width of 2 (base4) and 6 (base64)
    /// - 2 for a bit-width of 4 (hexadecimal)
    ///
    /// Wrapping is disabled if null.
    pub width: usize,

    /// Wrapping characters
    ///
    /// Wrapping is disabled if empty.
    pub separator: String,
}

/// Base-conversion specification
///
/// It is possible to define custom encodings given a specification. To do so, it is important to
/// understand the theory first.
///
/// # Theory
///
/// Each subsection has an equivalent subsection in the [Practice](#practice) section.
///
/// ## Basics
///
/// The main idea of a [base-conversion] encoding is to see `[u8]` as numbers written in
/// little-endian base256 and convert them in another little-endian base. For performance reasons,
/// this crate restricts this other base to be of size 2 (binary), 4 (base4), 8 (octal), 16
/// (hexadecimal), 32 (base32), or 64 (base64). The converted number is written as `[u8]` although
/// it doesn't use all the 256 possible values of `u8`. This crate encodes to ASCII, so only values
/// smaller than 128 are allowed.
///
/// More precisely, we need the following elements:
///
/// - The bit-width N: 1 for binary, 2 for base4, 3 for octal, 4 for hexadecimal, 5 for base32, and
///   6 for base64
/// - The [bit-order](enum.BitOrder.html): most or least significant bit first
/// - The symbols function S from [0, 2<sup>N</sup>) (called values and written `uN`) to symbols
///   (represented as `u8` although only ASCII symbols are allowed, i.e. smaller than 128)
/// - The values partial function V from ASCII to [0, 2<sup>N</sup>), i.e. from `u8` to `uN`
/// - Whether trailing bits are checked: trailing bits are leading zeros in theory, but since
///   numbers are little-endian they come last
///
/// For the encoding to be correct (i.e. encoding then decoding gives back the initial input),
/// V(S(i)) must be defined and equal to i for all i in [0, 2<sup>N</sup>). For the encoding to be
/// [canonical][canonical] (i.e. different inputs decode to different outputs, or equivalently,
/// decoding then encoding gives back the initial input), trailing bits must be checked and if V(i)
/// is defined then S(V(i)) is equal to i for all i.
///
/// Encoding and decoding are given by the following pipeline:
///
/// ```text
/// [u8] <--1--> [[bit; 8]] <--2--> [[bit; N]] <--3--> [uN] <--4--> [u8]
/// 1: Map bit-order between each u8 and [bit; 8]
/// 2: Base conversion between base 2^8 and base 2^N (check trailing bits)
/// 3: Map bit-order between each [bit; N] and uN
/// 4: Map symbols/values between each uN and u8 (values must be defined)
/// ```
///
/// ## Extensions
///
/// All these extensions make the encoding not canonical.
///
/// ### Padding
///
/// Padding is useful if the following conditions are met:
///
/// - the bit-width is 3 (octal), 5 (base32), or 6 (base64)
/// - the length of the data to encode is not known in advance
/// - the data must be sent without buffering
///
/// Bases for which the bit-width N does not divide 8 may not concatenate encoded data. This comes
/// from the fact that it is not possible to make the difference between trailing bits and encoding
/// bits. Padding solves this issue by adding a new character to discriminate between trailing bits
/// and encoding bits. The idea is to work by blocks of lcm(8, N) bits, where lcm(8, N) is the least
/// common multiple of 8 and N. When such block is not complete, it is padded.
///
/// To preserve correctness, the padding character must not be a symbol.
///
/// ### Ignore characters when decoding
///
/// Ignoring characters when decoding is useful if after encoding some characters are added for
/// convenience or any other reason (like wrapping). In that case we want to first ignore those
/// characters before decoding.
///
/// To preserve correctness, ignored characters must not contain symbols or the padding character.
///
/// ### Wrap output when encoding
///
/// Wrapping output when encoding is useful if the output is meant to be printed in a document where
/// width is limited (typically 80-columns documents). In that case, the wrapping width and the
/// wrapping separator have to be defined.
///
/// To preserve correctness, the wrapping separator characters must be ignored (see previous
/// subsection). As such, wrapping separator characters must also not contain symbols or the padding
/// character.
///
/// ### Translate characters when decoding
///
/// Translating characters when decoding is useful when encoded data may be copied by a humain
/// instead of a machine. Humans tend to confuse some characters for others. In that case we want to
/// translate those characters before decoding.
///
/// To preserve correctness, the characters we translate _from_ must not contain symbols or the
/// padding character, and the characters we translate _to_ must only contain symbols or the padding
/// character.
///
/// # Practice
///
/// ## Basics
///
/// ```rust
/// use data_encoding::{Encoding, Specification};
/// fn make_encoding(symbols: &str) -> Encoding {
///     let mut spec = Specification::new();
///     spec.symbols.push_str(symbols);
///     spec.encoding().unwrap()
/// }
/// let binary = make_encoding("01");
/// let octal = make_encoding("01234567");
/// let hexadecimal = make_encoding("0123456789abcdef");
/// assert_eq!(binary.encode(b"Bit"), "010000100110100101110100");
/// assert_eq!(octal.encode(b"Bit"), "20464564");
/// assert_eq!(hexadecimal.encode(b"Bit"), "426974");
/// ```
///
/// The `binary` base has 2 symbols `0` and `1` with value 0 and 1 respectively. The `octal` base
/// has 8 symbols `0` to `7` with value 0 to 7. The `hexadecimal` base has 16 symbols `0` to `9` and
/// `a` to `f` with value 0 to 15. The following diagram gives the idea of how encoding works in the
/// previous example (note that we can actually write such diagram only because the bit-order is
/// most significant first):
///
/// ```text
/// [      octal] |  2  :  0  :  4  :  6  :  4  :  5  :  6  :  4  |
/// [     binary] |0 1 0 0 0 0 1 0|0 1 1 0 1 0 0 1|0 1 1 1 0 1 0 0|
/// [hexadecimal] |   4   :   2   |   6   :   9   |   7   :   4   |
///                ^-- LSB                                       ^-- MSB
/// ```
///
/// Note that in theory, these little-endian numbers are read from right to left (the most
/// significant bit is at the right). Since leading zeros are meaningless (in our usual decimal
/// notation 0123 is the same as 123), it explains why trailing bits must be zero. Trailing bits may
/// occur when the bit-width of a base does not divide 8. Only binary, base4, and hexadecimal don't
/// have trailing bits issues. So let's consider octal and base64, which have trailing bits in
/// similar circumstances:
///
/// ```rust
/// use data_encoding::{Specification, BASE64_NOPAD};
/// let octal = {
///     let mut spec = Specification::new();
///     spec.symbols.push_str("01234567");
///     spec.encoding().unwrap()
/// };
/// assert_eq!(BASE64_NOPAD.encode(b"B"), "Qg");
/// assert_eq!(octal.encode(b"B"), "204");
/// ```
///
/// We have the following diagram, where the base64 values are written between parentheses:
///
/// ```text
/// [base64] |   Q(16)   :   g(32)   : [has 4 zero trailing bits]
/// [ octal] |  2  :  0  :  4  :       [has 1 zero trailing bit ]
///          |0 1 0 0 0 0 1 0|0 0 0 0
/// [ ascii] |       B       |
///                           ^-^-^-^-- leading zeros / trailing bits
/// ```
///
/// ## Extensions
///
/// ### Padding
///
/// For octal and base64, lcm(8, 3) == lcm(8, 6) == 24 bits or 3 bytes. For base32, lcm(8, 5) is 40
/// bits or 5 bytes. Let's consider octal and base64:
///
/// ```rust
/// use data_encoding::{Specification, BASE64};
/// let octal = {
///     let mut spec = Specification::new();
///     spec.symbols.push_str("01234567");
///     spec.padding = Some('=');
///     spec.encoding().unwrap()
/// };
/// // We start encoding but we only have "B" for now.
/// assert_eq!(BASE64.encode(b"B"), "Qg==");
/// assert_eq!(octal.encode(b"B"), "204=====");
/// // Now we have "it".
/// assert_eq!(BASE64.encode(b"it"), "aXQ=");
/// assert_eq!(octal.encode(b"it"), "322720==");
/// // By concatenating everything, we may decode the original data.
/// assert_eq!(BASE64.decode(b"Qg==aXQ=").unwrap(), b"Bit");
/// assert_eq!(octal.decode(b"204=====322720==").unwrap(), b"Bit");
/// ```
///
/// We have the following diagrams:
///
/// ```text
/// [base64] |   Q(16)   :   g(32)   :     =     :     =     |
/// [ octal] |  2  :  0  :  4  :  =  :  =  :  =  :  =  :  =  |
///          |0 1 0 0 0 0 1 0|. . . . . . . .|. . . . . . . .|
/// [ ascii] |       B       |        end of block aligned --^
///          ^-- beginning of block aligned
///
/// [base64] |   a(26)   :   X(23)   :   Q(16)   :     =     |
/// [ octal] |  3  :  2  :  2  :  7  :  2  :  0  :  =  :  =  |
///          |0 1 1 0 1 0 0 1|0 1 1 1 0 1 0 0|. . . . . . . .|
/// [ ascii] |       i       |       t       |
/// ```
///
/// ### Ignore characters when decoding
///
/// The typical use-case is to ignore newlines (`\r` and `\n`). But to keep the example small, we
/// will ignore spaces.
///
/// ```rust
/// let mut spec = data_encoding::HEXLOWER.specification();
/// spec.ignore.push_str(" \t");
/// let base = spec.encoding().unwrap();
/// assert_eq!(base.decode(b"42 69 74"), base.decode(b"426974"));
/// ```
///
/// ### Wrap output when encoding
///
/// The typical use-case is to wrap after 64 or 76 characters with a newline (`\r\n` or `\n`). But
/// to keep the example small, we will wrap after 8 characters with a space.
///
/// ```rust
/// let mut spec = data_encoding::BASE64.specification();
/// spec.wrap.width = 8;
/// spec.wrap.separator.push_str(" ");
/// let base64 = spec.encoding().unwrap();
/// assert_eq!(base64.encode(b"Hey you"), "SGV5IHlv dQ== ");
/// ```
///
/// Note that the output always ends with the separator.
///
/// ### Translate characters when decoding
///
/// The typical use-case is to translate lowercase to uppercase or reciprocally, but it is also used
/// for letters that look alike, like `O0` or `Il1`. Let's illustrate both examples.
///
/// ```rust
/// let mut spec = data_encoding::HEXLOWER.specification();
/// spec.translate.from.push_str("ABCDEFOIl");
/// spec.translate.to.push_str("abcdef011");
/// let base = spec.encoding().unwrap();
/// assert_eq!(base.decode(b"BOIl"), base.decode(b"b011"));
/// ```
///
/// [base-conversion]: https://en.wikipedia.org/wiki/Positional_notation#Base_conversion
/// [canonical]: https://tools.ietf.org/html/rfc4648#section-3.5
#[derive(Debug, Clone)]
#[cfg(feature = "alloc")]
pub struct Specification {
    /// Symbols
    ///
    /// The number of symbols must be 2, 4, 8, 16, 32, or 64. Symbols must be ASCII characters
    /// (smaller than 128) and they must be unique.
    pub symbols: String,

    /// Bit-order
    ///
    /// The default is to use most significant bit first since it is the most common.
    pub bit_order: BitOrder,

    /// Check trailing bits
    ///
    /// The default is to check trailing bits. This field is ignored when unnecessary (i.e. for
    /// base2, base4, and base16).
    pub check_trailing_bits: bool,

    /// Padding
    ///
    /// The default is to not use padding. The padding character must be ASCII and must not be a
    /// symbol.
    pub padding: Option<char>,

    /// Characters to ignore when decoding
    ///
    /// The default is to not ignore characters when decoding. The characters to ignore must be
    /// ASCII and must not be symbols or the padding character.
    pub ignore: String,

    /// How to wrap the output when encoding
    ///
    /// The default is to not wrap the output when encoding. The wrapping characters must be ASCII
    /// and must not be symbols or the padding character.
    pub wrap: Wrap,

    /// How to translate characters when decoding
    ///
    /// The default is to not translate characters when decoding. The characters to translate from
    /// must be ASCII and must not have already been assigned a semantics. The characters to
    /// translate to must be ASCII and must have been assigned a semantics (symbol, padding
    /// character, or ignored character).
    pub translate: Translate,
}

#[cfg(feature = "alloc")]
impl Default for Specification {
    fn default() -> Self {
        Self::new()
    }
}

impl Encoding {
    fn sym(&self) -> &[u8; 256] {
        self.0[0 .. 256].try_into().unwrap()
    }

    fn val(&self) -> &[u8; 256] {
        self.0[256 .. 512].try_into().unwrap()
    }

    fn pad(&self) -> Option<u8> {
        if self.0[512] < 128 {
            Some(self.0[512])
        } else {
            None
        }
    }

    fn ctb(&self) -> bool {
        self.0[513] & 0x10 != 0
    }

    fn msb(&self) -> bool {
        self.0[513] & 0x8 != 0
    }

    fn bit(&self) -> usize {
        (self.0[513] & 0x7) as usize
    }

    /// Minimum number of input and output blocks when encoding
    fn block_len(&self) -> (usize, usize) {
        let bit = self.bit();
        match self.wrap() {
            Some((col, end)) => (col / dec(bit) * enc(bit), col + end.len()),
            None => (enc(bit), dec(bit)),
        }
    }

    fn wrap(&self) -> Option<(usize, &[u8])> {
        if self.0.len() <= 515 {
            return None;
        }
        Some((self.0[514] as usize, &self.0[515 ..]))
    }

    fn has_ignore(&self) -> bool {
        self.0.len() >= 515
    }

    /// Returns the encoded length of an input of length `len`
    ///
    /// See [`encode_mut`] for when to use it.
    ///
    /// # Panics
    ///
    /// May panic if `len` is greater than `usize::MAX / 512`:
    /// - `len <= 8_388_607` when `target_pointer_width = "32"`
    /// - `len <= 36028_797018_963967` when `target_pointer_width = "64"`
    ///
    /// If you need to encode an input of length greater than this limit (possibly of infinite
    /// length), then you must chunk your input, encode each chunk, and concatenate to obtain the
    /// output. The length of each input chunk must be a multiple of [`encode_align`].
    ///
    /// Note that this function only _may_ panic in those cases. The function may also return the
    /// correct value in some cases depending on the implementation. In other words, those limits
    /// are the guarantee below which the function will not panic, and not the guarantee above which
    /// the function will panic.
    ///
    /// [`encode_align`]: struct.Encoding.html#method.encode_align
    /// [`encode_mut`]: struct.Encoding.html#method.encode_mut
    #[must_use]
    pub fn encode_len(&self, len: usize) -> usize {
        assert!(len <= usize::MAX / 512);
        dispatch! {
            let bit: usize = self.bit();
            let pad: Option<u8> = self.pad();
            let wrap: Option<(usize, &[u8])> = self.wrap();
            encode_wrap_len(bit, pad, wrap, len)
        }
    }

    /// Returns the minimum alignment when chunking a long input
    ///
    /// See [`encode_len`] for context.
    ///
    /// [`encode_len`]: struct.Encoding.html#method.encode_len
    #[must_use]
    pub fn encode_align(&self) -> usize {
        let bit = self.bit();
        match self.wrap() {
            None => enc(bit),
            Some((col, _)) => col * bit / 8,
        }
    }

    /// Encodes `input` in `output`
    ///
    /// # Panics
    ///
    /// Panics if the `output` length does not match the result of [`encode_len`] for the `input`
    /// length.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use data_encoding::BASE64;
    /// # let mut buffer = vec![0; 100];
    /// let input = b"Hello world";
    /// let output = &mut buffer[0 .. BASE64.encode_len(input.len())];
    /// BASE64.encode_mut(input, output);
    /// assert_eq!(output, b"SGVsbG8gd29ybGQ=");
    /// ```
    ///
    /// [`encode_len`]: struct.Encoding.html#method.encode_len
    #[allow(clippy::cognitive_complexity)]
    pub fn encode_mut(&self, input: &[u8], output: &mut [u8]) {
        assert_eq!(output.len(), self.encode_len(input.len()));
        dispatch! {
            let bit: usize = self.bit();
            let msb: bool = self.msb();
            let pad: Option<u8> = self.pad();
            let wrap: Option<(usize, &[u8])> = self.wrap();
            encode_wrap_mut(bit, msb, self.sym(), pad, wrap, input, output)
        }
    }

    /// Encodes `input` in `output` and returns it as a `&str`
    ///
    /// It is guaranteed that `output` and the return value only differ by their type. They both
    /// point to the same range of memory (pointer and length).
    ///
    /// # Panics
    ///
    /// Panics if the `output` length does not match the result of [`encode_len`] for the `input`
    /// length.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use data_encoding::BASE64;
    /// # let mut buffer = vec![0; 100];
    /// let input = b"Hello world";
    /// let output = &mut buffer[0 .. BASE64.encode_len(input.len())];
    /// assert_eq!(BASE64.encode_mut_str(input, output), "SGVsbG8gd29ybGQ=");
    /// ```
    ///
    /// [`encode_len`]: struct.Encoding.html#method.encode_len
    pub fn encode_mut_str<'a>(&self, input: &[u8], output: &'a mut [u8]) -> &'a str {
        self.encode_mut(input, output);
        safety_assert!(output.is_ascii());
        // SAFETY: Ensured by correctness guarantees of encode_mut (and asserted above).
        unsafe { core::str::from_utf8_unchecked(output) }
    }

    /// Appends the encoding of `input` to `output`
    ///
    /// # Examples
    ///
    /// ```rust
    /// use data_encoding::BASE64;
    /// # let mut buffer = vec![0; 100];
    /// let input = b"Hello world";
    /// let mut output = "Result: ".to_string();
    /// BASE64.encode_append(input, &mut output);
    /// assert_eq!(output, "Result: SGVsbG8gd29ybGQ=");
    /// ```
    #[cfg(feature = "alloc")]
    pub fn encode_append(&self, input: &[u8], output: &mut String) {
        // SAFETY: Ensured by correctness guarantees of encode_mut (and asserted below).
        let output = unsafe { output.as_mut_vec() };
        let output_len = output.len();
        output.resize(output_len + self.encode_len(input.len()), 0u8);
        self.encode_mut(input, &mut output[output_len ..]);
        safety_assert!(output[output_len ..].is_ascii());
    }

    /// Returns an object to encode a fragmented input and append it to `output`
    ///
    /// See the documentation of [`Encoder`] for more details and examples.
    #[cfg(feature = "alloc")]
    pub fn new_encoder<'a>(&'a self, output: &'a mut String) -> Encoder<'a> {
        Encoder::new(self, output)
    }

    /// Writes the encoding of `input` to `output`
    ///
    /// This allocates a buffer of 1024 bytes on the stack. If you want to control the buffer size
    /// and location, use [`Encoding::encode_write_buffer()`] instead.
    ///
    /// # Errors
    ///
    /// Returns an error when writing to the output fails.
    pub fn encode_write(
        &self, input: &[u8], output: &mut impl core::fmt::Write,
    ) -> core::fmt::Result {
        self.encode_write_buffer(input, output, &mut [0; 1024])
    }

    /// Writes the encoding of `input` to `output` using a temporary `buffer`
    ///
    /// # Panics
    ///
    /// Panics if the buffer is shorter than 510 bytes.
    ///
    /// # Errors
    ///
    /// Returns an error when writing to the output fails.
    pub fn encode_write_buffer(
        &self, input: &[u8], output: &mut impl core::fmt::Write, buffer: &mut [u8],
    ) -> core::fmt::Result {
        assert!(510 <= buffer.len());
        let (enc, dec) = self.block_len();
        for input in input.chunks(buffer.len() / dec * enc) {
            let buffer = &mut buffer[.. self.encode_len(input.len())];
            self.encode_mut(input, buffer);
            safety_assert!(buffer.is_ascii());
            // SAFETY: Ensured by correctness guarantees of encode_mut (and asserted above).
            output.write_str(unsafe { core::str::from_utf8_unchecked(buffer) })?;
        }
        Ok(())
    }

    /// Returns an object to display the encoding of `input`
    ///
    /// # Examples
    ///
    /// ```rust
    /// use data_encoding::BASE64;
    /// assert_eq!(
    ///     format!("Payload: {}", BASE64.encode_display(b"Hello world")),
    ///     "Payload: SGVsbG8gd29ybGQ=",
    /// );
    /// ```
    #[must_use]
    pub fn encode_display<'a>(&'a self, input: &'a [u8]) -> Display<'a> {
        Display { encoding: self, input }
    }

    /// Returns encoded `input`
    ///
    /// # Examples
    ///
    /// ```rust
    /// use data_encoding::BASE64;
    /// assert_eq!(BASE64.encode(b"Hello world"), "SGVsbG8gd29ybGQ=");
    /// ```
    #[cfg(feature = "alloc")]
    #[must_use]
    pub fn encode(&self, input: &[u8]) -> String {
        let mut output = vec![0u8; self.encode_len(input.len())];
        self.encode_mut(input, &mut output);
        safety_assert!(output.is_ascii());
        // SAFETY: Ensured by correctness guarantees of encode_mut (and asserted above).
        unsafe { String::from_utf8_unchecked(output) }
    }

    /// Returns the maximum decoded length of an input of length `len`
    ///
    /// See [`decode_mut`] for when to use it. In particular, the actual decoded length might be
    /// smaller if the actual input contains padding or ignored characters.
    ///
    /// # Panics
    ///
    /// May panic if `len` is greater than `usize::MAX / 8`:
    /// - `len <= 536_870_911` when `target_pointer_width = "32"`
    /// - `len <= 2_305843_009213_693951` when `target_pointer_width = "64"`
    ///
    /// If you need to decode an input of length greater than this limit (possibly of infinite
    /// length), then you must decode your input chunk by chunk with [`decode_mut`], making sure
    /// that you take into account how many bytes have been read from the input and how many bytes
    /// have been written to the output:
    /// - `Ok(written)` means all bytes have been read and `written` bytes have been written
    /// - `Err(DecodePartial { error, .. })` means an error occurred if `error.kind !=
    ///   DecodeKind::Length` or this was the last input chunk
    /// - `Err(DecodePartial { read, written, .. })` means that `read` bytes have been read and
    ///   `written` bytes written (the error can be ignored)
    ///
    /// Note that this function only _may_ panic in those cases. The function may also return the
    /// correct value in some cases depending on the implementation. In other words, those limits
    /// are the guarantee below which the function will not panic, and not the guarantee above which
    /// the function will panic.
    ///
    /// # Errors
    ///
    /// Returns an error if `len` is invalid. The error kind is [`Length`] and the [position] is the
    /// greatest valid input length.
    ///
    /// [`decode_mut`]: struct.Encoding.html#method.decode_mut
    /// [`Length`]: enum.DecodeKind.html#variant.Length
    /// [position]: struct.DecodeError.html#structfield.position
    pub fn decode_len(&self, len: usize) -> Result<usize, DecodeError> {
        assert!(len <= usize::MAX / 8);
        let (ilen, olen) = dispatch! {
            let bit: usize = self.bit();
            let pad: bool = self.pad().is_some();
            decode_wrap_len(bit, pad, len)
        };
        check!(
            DecodeError { position: ilen, kind: DecodeKind::Length },
            self.has_ignore() || len == ilen
        );
        Ok(olen)
    }

    /// Decodes `input` in `output`
    ///
    /// Returns the length of the decoded output. This length may be smaller than the output length
    /// if the input contained padding or ignored characters. The output bytes after the returned
    /// length are not initialized and should not be read.
    ///
    /// # Panics
    ///
    /// Panics if the `output` length does not match the result of [`decode_len`] for the `input`
    /// length. Also panics if `decode_len` fails for the `input` length.
    ///
    /// # Errors
    ///
    /// Returns an error if `input` is invalid. See [`decode`] for more details. The are two
    /// differences though:
    ///
    /// - [`Length`] may be returned only if the encoding allows ignored characters, because
    ///   otherwise this is already checked by [`decode_len`].
    /// - The [`read`] first bytes of the input have been successfully decoded to the [`written`]
    ///   first bytes of the output.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use data_encoding::BASE64;
    /// # let mut buffer = vec![0; 100];
    /// let input = b"SGVsbA==byB3b3JsZA==";
    /// let output = &mut buffer[0 .. BASE64.decode_len(input.len()).unwrap()];
    /// let len = BASE64.decode_mut(input, output).unwrap();
    /// assert_eq!(&output[0 .. len], b"Hello world");
    /// ```
    ///
    /// [`decode_len`]: struct.Encoding.html#method.decode_len
    /// [`decode`]: struct.Encoding.html#method.decode
    /// [`Length`]: enum.DecodeKind.html#variant.Length
    /// [`read`]: struct.DecodePartial.html#structfield.read
    /// [`written`]: struct.DecodePartial.html#structfield.written
    #[allow(clippy::cognitive_complexity)]
    pub fn decode_mut(&self, input: &[u8], output: &mut [u8]) -> Result<usize, DecodePartial> {
        assert_eq!(Ok(output.len()), self.decode_len(input.len()));
        dispatch! {
            let bit: usize = self.bit();
            let msb: bool = self.msb();
            let pad: bool = self.pad().is_some();
            let has_ignore: bool = self.has_ignore();
            decode_wrap_mut(bit, msb, self.ctb(), self.val(), pad, has_ignore,
                            input, output)
        }
    }

    /// Returns decoded `input`
    ///
    /// # Errors
    ///
    /// Returns an error if `input` is invalid. The error kind can be:
    ///
    /// - [`Length`] if the input length is invalid. The [position] is the greatest valid input
    ///   length.
    /// - [`Symbol`] if the input contains an invalid character. The [position] is the first invalid
    ///   character.
    /// - [`Trailing`] if the input has non-zero trailing bits. This is only possible if the
    ///   encoding checks trailing bits. The [position] is the first character containing non-zero
    ///   trailing bits.
    /// - [`Padding`] if the input has an invalid padding length. This is only possible if the
    ///   encoding uses padding. The [position] is the first padding character of the first padding
    ///   of invalid length.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use data_encoding::BASE64;
    /// assert_eq!(BASE64.decode(b"SGVsbA==byB3b3JsZA==").unwrap(), b"Hello world");
    /// ```
    ///
    /// [`Length`]: enum.DecodeKind.html#variant.Length
    /// [`Symbol`]: enum.DecodeKind.html#variant.Symbol
    /// [`Trailing`]: enum.DecodeKind.html#variant.Trailing
    /// [`Padding`]: enum.DecodeKind.html#variant.Padding
    /// [position]: struct.DecodeError.html#structfield.position
    #[cfg(feature = "alloc")]
    pub fn decode(&self, input: &[u8]) -> Result<Vec<u8>, DecodeError> {
        let mut output = vec![0u8; self.decode_len(input.len())?];
        let len = self.decode_mut(input, &mut output).map_err(|partial| partial.error)?;
        output.truncate(len);
        Ok(output)
    }

    /// Returns the bit-width
    #[must_use]
    pub fn bit_width(&self) -> usize {
        self.bit()
    }

    /// Returns whether the encoding is canonical
    ///
    /// An encoding is not canonical if one of the following conditions holds:
    ///
    /// - trailing bits are not checked
    /// - padding is used
    /// - characters are ignored
    /// - characters are translated
    #[must_use]
    pub fn is_canonical(&self) -> bool {
        if !self.ctb() {
            return false;
        }
        let bit = self.bit();
        let sym = self.sym();
        let val = self.val();
        for i in 0 .. 256 {
            if val[i] == INVALID {
                continue;
            }
            if val[i] >= 1 << bit {
                return false;
            }
            if sym[val[i] as usize] as usize != i {
                return false;
            }
        }
        true
    }

    /// Returns the encoding specification
    #[allow(clippy::missing_panics_doc)] // no panic
    #[cfg(feature = "alloc")]
    #[must_use]
    pub fn specification(&self) -> Specification {
        let mut specification = Specification::new();
        specification
            .symbols
            .push_str(core::str::from_utf8(&self.sym()[0 .. 1 << self.bit()]).unwrap());
        specification.bit_order =
            if self.msb() { MostSignificantFirst } else { LeastSignificantFirst };
        specification.check_trailing_bits = self.ctb();
        if let Some(pad) = self.pad() {
            specification.padding = Some(pad as char);
        }
        for i in 0 .. 128u8 {
            if self.val()[i as usize] != IGNORE {
                continue;
            }
            specification.ignore.push(i as char);
        }
        if let Some((col, end)) = self.wrap() {
            specification.wrap.width = col;
            specification.wrap.separator = core::str::from_utf8(end).unwrap().to_owned();
        }
        for i in 0 .. 128u8 {
            let canonical = if self.val()[i as usize] < 1 << self.bit() {
                self.sym()[self.val()[i as usize] as usize]
            } else if self.val()[i as usize] == PADDING {
                self.pad().unwrap()
            } else {
                continue;
            };
            if i == canonical {
                continue;
            }
            specification.translate.from.push(i as char);
            specification.translate.to.push(canonical as char);
        }
        specification
    }

    #[doc(hidden)]
    #[must_use]
    pub const fn internal_new(implementation: &'static [u8]) -> Encoding {
        #[cfg(feature = "alloc")]
        let encoding = Encoding(Cow::Borrowed(implementation));
        #[cfg(not(feature = "alloc"))]
        let encoding = Encoding(implementation);
        encoding
    }

    #[doc(hidden)]
    #[must_use]
    pub fn internal_implementation(&self) -> &[u8] {
        &self.0
    }
}

/// Encodes fragmented input to an output
///
/// It is equivalent to use an [`Encoder`] with multiple calls to [`Encoder::append()`] than to
/// first concatenate all the input and then use [`Encoding::encode_append()`]. In particular, this
/// function will not introduce padding or wrapping between inputs.
///
/// # Examples
///
/// ```rust
/// // This is a bit inconvenient but we can't take a long-term reference to data_encoding::BASE64
/// // because it's a constant. We need to use a static which has an address instead. This will be
/// // fixed in version 3 of the library.
/// static BASE64: data_encoding::Encoding = data_encoding::BASE64;
/// let mut output = String::new();
/// let mut encoder = BASE64.new_encoder(&mut output);
/// encoder.append(b"hello");
/// encoder.append(b"world");
/// encoder.finalize();
/// assert_eq!(output, BASE64.encode(b"helloworld"));
/// ```
#[derive(Debug)]
#[cfg(feature = "alloc")]
pub struct Encoder<'a> {
    encoding: &'a Encoding,
    output: &'a mut String,
    buffer: [u8; 255],
    length: u8,
}

#[cfg(feature = "alloc")]
impl Drop for Encoder<'_> {
    fn drop(&mut self) {
        self.encoding.encode_append(&self.buffer[.. self.length as usize], self.output);
    }
}

#[cfg(feature = "alloc")]
impl<'a> Encoder<'a> {
    fn new(encoding: &'a Encoding, output: &'a mut String) -> Self {
        Encoder { encoding, output, buffer: [0; 255], length: 0 }
    }

    /// Encodes the provided input fragment and appends the result to the output
    pub fn append(&mut self, mut input: &[u8]) {
        #[allow(clippy::cast_possible_truncation)] // no truncation
        let max = self.encoding.block_len().0 as u8;
        if self.length != 0 {
            let len = self.length;
            #[allow(clippy::cast_possible_truncation)] // no truncation
            let add = core::cmp::min((max - len) as usize, input.len()) as u8;
            self.buffer[len as usize ..][.. add as usize].copy_from_slice(&input[.. add as usize]);
            self.length += add;
            input = &input[add as usize ..];
            if self.length != max {
                debug_assert!(self.length < max);
                debug_assert!(input.is_empty());
                return;
            }
            self.encoding.encode_append(&self.buffer[.. max as usize], self.output);
            self.length = 0;
        }
        let len = floor(input.len(), max as usize);
        self.encoding.encode_append(&input[.. len], self.output);
        input = &input[len ..];
        #[allow(clippy::cast_possible_truncation)] // no truncation
        let len = input.len() as u8;
        self.buffer[.. len as usize].copy_from_slice(input);
        self.length = len;
    }

    /// Makes sure all inputs have been encoded and appended to the output
    ///
    /// This is equivalent to dropping the encoder and required for correctness, otherwise some
    /// encoded data may be missing at the end.
    pub fn finalize(self) {}
}

/// Wraps an encoding and input for display purposes.
#[derive(Debug)]
pub struct Display<'a> {
    encoding: &'a Encoding,
    input: &'a [u8],
}

impl core::fmt::Display for Display<'_> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        self.encoding.encode_write(self.input, f)
    }
}

#[derive(Debug, Copy, Clone)]
#[cfg(feature = "alloc")]
enum SpecificationErrorImpl {
    BadSize,
    NotAscii,
    Duplicate(u8),
    ExtraPadding,
    WrapLength,
    WrapWidth(u8),
    FromTo,
    Undefined(u8),
}
#[cfg(feature = "alloc")]
use crate::SpecificationErrorImpl::*;

/// Specification error
#[derive(Debug, Copy, Clone)]
#[cfg(feature = "alloc")]
pub struct SpecificationError(SpecificationErrorImpl);

#[cfg(feature = "alloc")]
impl core::fmt::Display for SpecificationError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self.0 {
            BadSize => write!(f, "invalid number of symbols"),
            NotAscii => write!(f, "non-ascii character"),
            Duplicate(c) => write!(f, "{:?} has conflicting definitions", c as char),
            ExtraPadding => write!(f, "unnecessary padding"),
            WrapLength => write!(f, "invalid wrap width or separator length"),
            WrapWidth(x) => write!(f, "wrap width not a multiple of {}", x),
            FromTo => write!(f, "translate from/to length mismatch"),
            Undefined(c) => write!(f, "{:?} is undefined", c as char),
        }
    }
}

#[cfg(feature = "std")]
impl std::error::Error for SpecificationError {
    fn description(&self) -> &str {
        match self.0 {
            BadSize => "invalid number of symbols",
            NotAscii => "non-ascii character",
            Duplicate(_) => "conflicting definitions",
            ExtraPadding => "unnecessary padding",
            WrapLength => "invalid wrap width or separator length",
            WrapWidth(_) => "wrap width not a multiple",
            FromTo => "translate from/to length mismatch",
            Undefined(_) => "undefined character",
        }
    }
}

#[cfg(feature = "alloc")]
impl Specification {
    /// Returns a default specification
    #[must_use]
    pub fn new() -> Specification {
        Specification {
            symbols: String::new(),
            bit_order: MostSignificantFirst,
            check_trailing_bits: true,
            padding: None,
            ignore: String::new(),
            wrap: Wrap { width: 0, separator: String::new() },
            translate: Translate { from: String::new(), to: String::new() },
        }
    }

    /// Returns the specified encoding
    ///
    /// # Errors
    ///
    /// Returns an error if the specification is invalid.
    pub fn encoding(&self) -> Result<Encoding, SpecificationError> {
        let symbols = self.symbols.as_bytes();
        let bit: u8 = match symbols.len() {
            2 => 1,
            4 => 2,
            8 => 3,
            16 => 4,
            32 => 5,
            64 => 6,
            _ => return Err(SpecificationError(BadSize)),
        };
        let mut values = [INVALID; 128];
        let set = |v: &mut [u8; 128], i: u8, x: u8| {
            check!(SpecificationError(NotAscii), i < 128);
            if v[i as usize] == x {
                return Ok(());
            }
            check!(SpecificationError(Duplicate(i)), v[i as usize] == INVALID);
            v[i as usize] = x;
            Ok(())
        };
        for (v, symbols) in symbols.iter().enumerate() {
            #[allow(clippy::cast_possible_truncation)] // no truncation
            set(&mut values, *symbols, v as u8)?;
        }
        let msb = self.bit_order == MostSignificantFirst;
        let ctb = self.check_trailing_bits || 8 % bit == 0;
        let pad = match self.padding {
            None => None,
            Some(pad) => {
                check!(SpecificationError(ExtraPadding), 8 % bit != 0);
                check!(SpecificationError(NotAscii), pad.len_utf8() == 1);
                set(&mut values, pad as u8, PADDING)?;
                Some(pad as u8)
            }
        };
        for i in self.ignore.bytes() {
            set(&mut values, i, IGNORE)?;
        }
        let wrap = if self.wrap.separator.is_empty() || self.wrap.width == 0 {
            None
        } else {
            let col = self.wrap.width;
            let end = self.wrap.separator.as_bytes();
            check!(SpecificationError(WrapLength), col < 256 && end.len() < 256);
            #[allow(clippy::cast_possible_truncation)] // no truncation
            let col = col as u8;
            #[allow(clippy::cast_possible_truncation)] // no truncation
            let dec = dec(bit as usize) as u8;
            check!(SpecificationError(WrapWidth(dec)), col % dec == 0);
            for &i in end {
                set(&mut values, i, IGNORE)?;
            }
            Some((col, end))
        };
        let from = self.translate.from.as_bytes();
        let to = self.translate.to.as_bytes();
        check!(SpecificationError(FromTo), from.len() == to.len());
        for i in 0 .. from.len() {
            check!(SpecificationError(NotAscii), to[i] < 128);
            let v = values[to[i] as usize];
            check!(SpecificationError(Undefined(to[i])), v != INVALID);
            set(&mut values, from[i], v)?;
        }
        let mut encoding = Vec::new();
        for _ in 0 .. 256 / symbols.len() {
            encoding.extend_from_slice(symbols);
        }
        encoding.extend_from_slice(&values);
        encoding.extend_from_slice(&[INVALID; 128]);
        match pad {
            None => encoding.push(INVALID),
            Some(pad) => encoding.push(pad),
        }
        encoding.push(bit);
        if msb {
            encoding[513] |= 0x08;
        }
        if ctb {
            encoding[513] |= 0x10;
        }
        if let Some((col, end)) = wrap {
            encoding.push(col);
            encoding.extend_from_slice(end);
        } else if values.contains(&IGNORE) {
            encoding.push(0);
        }
        Ok(Encoding(Cow::Owned(encoding)))
    }
}

/// Lowercase hexadecimal encoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, HEXLOWER};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("0123456789abcdef");
/// assert_eq!(HEXLOWER, spec.encoding().unwrap());
/// ```
///
/// # Examples
///
/// ```rust
/// use data_encoding::HEXLOWER;
/// let deadbeef = vec![0xde, 0xad, 0xbe, 0xef];
/// assert_eq!(HEXLOWER.decode(b"deadbeef").unwrap(), deadbeef);
/// assert_eq!(HEXLOWER.encode(&deadbeef), "deadbeef");
/// ```
pub const HEXLOWER: Encoding = Encoding::internal_new(HEXLOWER_IMPL);
const HEXLOWER_IMPL: &[u8] = &[
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54,
    55, 56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100,
    101, 102, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51,
    52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97,
    98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100,
    101, 102, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51,
    52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97,
    98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 97, 98, 99, 100, 101, 102, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 0, 1, 2,
    3, 4, 5, 6, 7, 8, 9, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 10, 11, 12, 13, 14, 15, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 28,
];

/// Lowercase hexadecimal encoding with case-insensitive decoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, HEXLOWER_PERMISSIVE};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("0123456789abcdef");
/// spec.translate.from.push_str("ABCDEF");
/// spec.translate.to.push_str("abcdef");
/// assert_eq!(HEXLOWER_PERMISSIVE, spec.encoding().unwrap());
/// ```
///
/// # Examples
///
/// ```rust
/// use data_encoding::HEXLOWER_PERMISSIVE;
/// let deadbeef = vec![0xde, 0xad, 0xbe, 0xef];
/// assert_eq!(HEXLOWER_PERMISSIVE.decode(b"DeadBeef").unwrap(), deadbeef);
/// assert_eq!(HEXLOWER_PERMISSIVE.encode(&deadbeef), "deadbeef");
/// ```
///
/// You can also define a shorter name:
///
/// ```rust
/// use data_encoding::{Encoding, HEXLOWER_PERMISSIVE};
/// const HEX: Encoding = HEXLOWER_PERMISSIVE;
/// ```
pub const HEXLOWER_PERMISSIVE: Encoding = Encoding::internal_new(HEXLOWER_PERMISSIVE_IMPL);
const HEXLOWER_PERMISSIVE_IMPL: &[u8] = &[
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54,
    55, 56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100,
    101, 102, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51,
    52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97,
    98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100,
    101, 102, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51,
    52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97,
    98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 97, 98, 99, 100, 101, 102, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 0, 1, 2,
    3, 4, 5, 6, 7, 8, 9, 128, 128, 128, 128, 128, 128, 128, 10, 11, 12, 13, 14, 15, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 10, 11, 12, 13, 14, 15, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 28,
];

/// Uppercase hexadecimal encoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, HEXUPPER};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("0123456789ABCDEF");
/// assert_eq!(HEXUPPER, spec.encoding().unwrap());
/// ```
///
/// It is compliant with [RFC4648] and known as "base16" or "hex".
///
/// # Examples
///
/// ```rust
/// use data_encoding::HEXUPPER;
/// let deadbeef = vec![0xde, 0xad, 0xbe, 0xef];
/// assert_eq!(HEXUPPER.decode(b"DEADBEEF").unwrap(), deadbeef);
/// assert_eq!(HEXUPPER.encode(&deadbeef), "DEADBEEF");
/// ```
///
/// [RFC4648]: https://tools.ietf.org/html/rfc4648#section-8
pub const HEXUPPER: Encoding = Encoding::internal_new(HEXUPPER_IMPL);
const HEXUPPER_IMPL: &[u8] = &[
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 128, 128, 128, 128, 128, 128, 128, 10, 11,
    12, 13, 14, 15, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 28,
];

/// Uppercase hexadecimal encoding with case-insensitive decoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, HEXUPPER_PERMISSIVE};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("0123456789ABCDEF");
/// spec.translate.from.push_str("abcdef");
/// spec.translate.to.push_str("ABCDEF");
/// assert_eq!(HEXUPPER_PERMISSIVE, spec.encoding().unwrap());
/// ```
///
/// # Examples
///
/// ```rust
/// use data_encoding::HEXUPPER_PERMISSIVE;
/// let deadbeef = vec![0xde, 0xad, 0xbe, 0xef];
/// assert_eq!(HEXUPPER_PERMISSIVE.decode(b"DeadBeef").unwrap(), deadbeef);
/// assert_eq!(HEXUPPER_PERMISSIVE.encode(&deadbeef), "DEADBEEF");
/// ```
pub const HEXUPPER_PERMISSIVE: Encoding = Encoding::internal_new(HEXUPPER_PERMISSIVE_IMPL);
const HEXUPPER_PERMISSIVE_IMPL: &[u8] = &[
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 65, 66, 67, 68, 69, 70, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 128, 128, 128, 128, 128, 128, 128, 10, 11,
    12, 13, 14, 15, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 10, 11, 12, 13, 14, 15, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 28,
];

/// Padded base32 encoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, BASE32};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567");
/// spec.padding = Some('=');
/// assert_eq!(BASE32, spec.encoding().unwrap());
/// ```
///
/// It conforms to [RFC4648].
///
/// [RFC4648]: https://tools.ietf.org/html/rfc4648#section-6
pub const BASE32: Encoding = Encoding::internal_new(BASE32_IMPL);
const BASE32_IMPL: &[u8] = &[
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
    81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72,
    73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55,
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
    81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72,
    73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55,
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
    81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 26, 27, 28, 29, 30, 31, 128, 128, 128, 128, 128, 130, 128, 128,
    128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 61, 29,
];

/// Unpadded base32 encoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, BASE32_NOPAD};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567");
/// assert_eq!(BASE32_NOPAD, spec.encoding().unwrap());
/// ```
pub const BASE32_NOPAD: Encoding = Encoding::internal_new(BASE32_NOPAD_IMPL);
const BASE32_NOPAD_IMPL: &[u8] = &[
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
    81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72,
    73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55,
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
    81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72,
    73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55,
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
    81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 26, 27, 28, 29, 30, 31, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 29,
];

/// Unpadded base32 encoding with case-insensitive decoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, BASE32_NOPAD_NOCASE};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567");
/// spec.translate.from.push_str("abcdefghijklmnopqrstuvwxyz");
/// spec.translate.to.push_str("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
/// assert_eq!(BASE32_NOPAD_NOCASE, spec.encoding().unwrap());
/// ```
pub const BASE32_NOPAD_NOCASE: Encoding = Encoding::internal_new(BASE32_NOPAD_NOCASE_IMPL);
const BASE32_NOPAD_NOCASE_IMPL: &[u8] = &[
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
    81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72,
    73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55,
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
    81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72,
    73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55,
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
    81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 26, 27, 28, 29, 30, 31, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 128, 128, 128, 128, 128, 128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
    18, 19, 20, 21, 22, 23, 24, 25, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 29,
];

/// Unpadded base32 encoding with visual error correction during decoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, BASE32_NOPAD_VISUAL};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567");
/// spec.translate.from.push_str("01l8");
/// spec.translate.to.push_str("OIIB");
/// assert_eq!(BASE32_NOPAD_VISUAL, spec.encoding().unwrap());
/// ```
pub const BASE32_NOPAD_VISUAL: Encoding = Encoding::internal_new(BASE32_NOPAD_VISUAL_IMPL);
const BASE32_NOPAD_VISUAL_IMPL: &[u8] = &[
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
    81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72,
    73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55,
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
    81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72,
    73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55,
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 50, 51, 52, 53, 54, 55, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
    81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 50, 51, 52, 53, 54, 55, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 14, 8, 26, 27, 28, 29, 30, 31, 1, 128, 128, 128, 128, 128, 128, 128, 128,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 8, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 29,
];

/// Padded base32hex encoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, BASE32HEX};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("0123456789ABCDEFGHIJKLMNOPQRSTUV");
/// spec.padding = Some('=');
/// assert_eq!(BASE32HEX, spec.encoding().unwrap());
/// ```
///
/// It conforms to [RFC4648].
///
/// [RFC4648]: https://tools.ietf.org/html/rfc4648#section-7
pub const BASE32HEX: Encoding = Encoding::internal_new(BASE32HEX_IMPL);
const BASE32HEX_IMPL: &[u8] = &[
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
    79, 80, 81, 82, 83, 84, 85, 86, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70,
    71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
    79, 80, 81, 82, 83, 84, 85, 86, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70,
    71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
    79, 80, 81, 82, 83, 84, 85, 86, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70,
    71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 128, 128, 128, 130, 128, 128, 128, 10, 11,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 61, 29,
];

/// Unpadded base32hex encoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, BASE32HEX_NOPAD};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("0123456789ABCDEFGHIJKLMNOPQRSTUV");
/// assert_eq!(BASE32HEX_NOPAD, spec.encoding().unwrap());
/// ```
pub const BASE32HEX_NOPAD: Encoding = Encoding::internal_new(BASE32HEX_NOPAD_IMPL);
const BASE32HEX_NOPAD_IMPL: &[u8] = &[
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
    79, 80, 81, 82, 83, 84, 85, 86, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70,
    71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
    79, 80, 81, 82, 83, 84, 85, 86, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70,
    71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
    79, 80, 81, 82, 83, 84, 85, 86, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 65, 66, 67, 68, 69, 70,
    71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 128, 128, 128, 128, 128, 128, 128, 10, 11,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 29,
];

/// DNSSEC base32 encoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, BASE32_DNSSEC};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("0123456789abcdefghijklmnopqrstuv");
/// spec.translate.from.push_str("ABCDEFGHIJKLMNOPQRSTUV");
/// spec.translate.to.push_str("abcdefghijklmnopqrstuv");
/// assert_eq!(BASE32_DNSSEC, spec.encoding().unwrap());
/// ```
///
/// It conforms to [RFC5155]:
///
/// - It uses a base32 extended hex alphabet.
/// - It is case-insensitive when decoding and uses lowercase when encoding.
/// - It does not use padding.
///
/// [RFC5155]: https://tools.ietf.org/html/rfc5155
pub const BASE32_DNSSEC: Encoding = Encoding::internal_new(BASE32_DNSSEC_IMPL);
const BASE32_DNSSEC_IMPL: &[u8] = &[
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107,
    108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
    97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
    116, 117, 118, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 103, 104,
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 48, 49, 50, 51, 52, 53,
    54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112,
    113, 114, 115, 116, 117, 118, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101,
    102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 48, 49,
    50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
    110, 111, 112, 113, 114, 115, 116, 117, 118, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98,
    99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
    118, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106,
    107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 128, 128, 128, 128, 128, 128, 128, 10, 11, 12, 13,
    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
    26, 27, 28, 29, 30, 31, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 29,
];

#[allow(clippy::doc_markdown)]
/// DNSCurve base32 encoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{BitOrder, Specification, BASE32_DNSCURVE};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("0123456789bcdfghjklmnpqrstuvwxyz");
/// spec.bit_order = BitOrder::LeastSignificantFirst;
/// spec.translate.from.push_str("BCDFGHJKLMNPQRSTUVWXYZ");
/// spec.translate.to.push_str("bcdfghjklmnpqrstuvwxyz");
/// assert_eq!(BASE32_DNSCURVE, spec.encoding().unwrap());
/// ```
///
/// It conforms to [DNSCurve].
///
/// [DNSCurve]: https://dnscurve.org/in-implement.html
pub const BASE32_DNSCURVE: Encoding = Encoding::internal_new(BASE32_DNSCURVE_IMPL);
const BASE32_DNSCURVE_IMPL: &[u8] = &[
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 98, 99, 100, 102, 103, 104, 106, 107, 108, 109, 110,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
    98, 99, 100, 102, 103, 104, 106, 107, 108, 109, 110, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 98, 99, 100, 102, 103, 104, 106, 107,
    108, 109, 110, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53,
    54, 55, 56, 57, 98, 99, 100, 102, 103, 104, 106, 107, 108, 109, 110, 112, 113, 114, 115, 116,
    117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 98, 99, 100, 102, 103,
    104, 106, 107, 108, 109, 110, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 48, 49,
    50, 51, 52, 53, 54, 55, 56, 57, 98, 99, 100, 102, 103, 104, 106, 107, 108, 109, 110, 112, 113,
    114, 115, 116, 117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 98, 99,
    100, 102, 103, 104, 106, 107, 108, 109, 110, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
    122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 98, 99, 100, 102, 103, 104, 106, 107, 108, 109,
    110, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 128, 128, 128, 128, 128, 128, 128, 128, 10, 11,
    12, 128, 13, 14, 15, 128, 16, 17, 18, 19, 20, 128, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    128, 128, 128, 128, 128, 128, 128, 10, 11, 12, 128, 13, 14, 15, 128, 16, 17, 18, 19, 20, 128,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 21,
];

/// Padded base64 encoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, BASE64};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
/// spec.padding = Some('=');
/// assert_eq!(BASE64, spec.encoding().unwrap());
/// ```
///
/// It conforms to [RFC4648].
///
/// [RFC4648]: https://tools.ietf.org/html/rfc4648#section-4
pub const BASE64: Encoding = Encoding::internal_new(BASE64_IMPL);
const BASE64_IMPL: &[u8] = &[
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
    115, 116, 117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43, 47, 65, 66,
    67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,
    97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
    116, 117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43, 47, 65, 66, 67,
    68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 97,
    98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116,
    117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43, 47, 65, 66, 67, 68,
    69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 97, 98,
    99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
    118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43, 47, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 62, 128, 128, 128, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 128, 128, 128, 130, 128,
    128, 128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 128, 128, 128, 128, 128, 128, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 61, 30,
];

/// Unpadded base64 encoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, BASE64_NOPAD};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
/// assert_eq!(BASE64_NOPAD, spec.encoding().unwrap());
/// ```
pub const BASE64_NOPAD: Encoding = Encoding::internal_new(BASE64_NOPAD_IMPL);
const BASE64_NOPAD_IMPL: &[u8] = &[
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
    115, 116, 117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43, 47, 65, 66,
    67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,
    97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
    116, 117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43, 47, 65, 66, 67,
    68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 97,
    98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116,
    117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43, 47, 65, 66, 67, 68,
    69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 97, 98,
    99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
    118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43, 47, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 62, 128, 128, 128, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 128, 128, 128, 128, 128,
    128, 128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 128, 128, 128, 128, 128, 128, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 30,
];

/// MIME base64 encoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, BASE64_MIME};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
/// spec.padding = Some('=');
/// spec.wrap.width = 76;
/// spec.wrap.separator.push_str("\r\n");
/// assert_eq!(BASE64_MIME, spec.encoding().unwrap());
/// ```
///
/// It does not exactly conform to [RFC2045] because it does not print the header
/// and does not ignore all characters.
///
/// [RFC2045]: https://tools.ietf.org/html/rfc2045
pub const BASE64_MIME: Encoding = Encoding::internal_new(BASE64_MIME_IMPL);
const BASE64_MIME_IMPL: &[u8] = &[
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
    115, 116, 117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43, 47, 65, 66,
    67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,
    97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
    116, 117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43, 47, 65, 66, 67,
    68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 97,
    98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116,
    117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43, 47, 65, 66, 67, 68,
    69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 97, 98,
    99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
    118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43, 47, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 129, 128, 128, 129, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 62, 128, 128, 128, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 128, 128, 128, 130, 128,
    128, 128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 128, 128, 128, 128, 128, 128, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 61, 30, 76, 13, 10,
];

/// MIME base64 encoding without trailing bits check
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, BASE64_MIME_PERMISSIVE};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
/// spec.padding = Some('=');
/// spec.wrap.width = 76;
/// spec.wrap.separator.push_str("\r\n");
/// spec.check_trailing_bits = false;
/// assert_eq!(BASE64_MIME_PERMISSIVE, spec.encoding().unwrap());
/// ```
///
/// It does not exactly conform to [RFC2045] because it does not print the header
/// and does not ignore all characters.
///
/// [RFC2045]: https://tools.ietf.org/html/rfc2045
pub const BASE64_MIME_PERMISSIVE: Encoding = Encoding::internal_new(BASE64_MIME_PERMISSIVE_IMPL);
const BASE64_MIME_PERMISSIVE_IMPL: &[u8] = &[
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
    115, 116, 117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43, 47, 65, 66,
    67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,
    97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
    116, 117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43, 47, 65, 66, 67,
    68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 97,
    98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116,
    117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43, 47, 65, 66, 67, 68,
    69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 97, 98,
    99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
    118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43, 47, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 129, 128, 128, 129, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 62, 128, 128, 128, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 128, 128, 128, 130, 128,
    128, 128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 128, 128, 128, 128, 128, 128, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 61, 14, 76, 13, 10,
];

/// Padded base64url encoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, BASE64URL};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
/// spec.padding = Some('=');
/// assert_eq!(BASE64URL, spec.encoding().unwrap());
/// ```
///
/// It conforms to [RFC4648].
///
/// [RFC4648]: https://tools.ietf.org/html/rfc4648#section-5
pub const BASE64URL: Encoding = Encoding::internal_new(BASE64URL_IMPL);
const BASE64URL_IMPL: &[u8] = &[
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
    115, 116, 117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 45, 95, 65, 66,
    67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,
    97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
    116, 117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 45, 95, 65, 66, 67,
    68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 97,
    98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116,
    117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 45, 95, 65, 66, 67, 68,
    69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 97, 98,
    99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
    118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 45, 95, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 62, 128, 128, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 128, 128, 128, 130, 128,
    128, 128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 128, 128, 128, 128, 63, 128, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 61, 30,
];

/// Unpadded base64url encoding
///
/// This encoding is a static version of:
///
/// ```rust
/// # use data_encoding::{Specification, BASE64URL_NOPAD};
/// let mut spec = Specification::new();
/// spec.symbols.push_str("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
/// assert_eq!(BASE64URL_NOPAD, spec.encoding().unwrap());
/// ```
pub const BASE64URL_NOPAD: Encoding = Encoding::internal_new(BASE64URL_NOPAD_IMPL);
const BASE64URL_NOPAD_IMPL: &[u8] = &[
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
    115, 116, 117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 45, 95, 65, 66,
    67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,
    97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
    116, 117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 45, 95, 65, 66, 67,
    68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 97,
    98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116,
    117, 118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 45, 95, 65, 66, 67, 68,
    69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 97, 98,
    99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
    118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 45, 95, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 62, 128, 128, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 128, 128, 128, 128, 128,
    128, 128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 128, 128, 128, 128, 63, 128, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
    128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 30,
];
