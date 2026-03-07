use swc_common::BytePos;

pub struct LazyInteger {
    pub(super) start: BytePos,
    pub(super) end: BytePos,
    /// `true` if there was `8` or `9``
    pub(super) not_octal: bool,
    pub(super) has_underscore: bool,
}

const MAX_SAFE_INT: u64 = 9007199254740991;

pub(super) fn parse_integer<const RADIX: u8>(s: &str) -> f64 {
    debug_assert!(matches!(RADIX, 2 | 8 | 10 | 16));
    debug_assert!(!s.is_empty());
    debug_assert!(!s.contains('_'));

    if RADIX == 10 {
        parse_integer_from_dec(s)
    } else if RADIX == 16 {
        parse_integer_from_hex(s)
    } else if RADIX == 2 {
        parse_integer_from_bin(s)
    } else if RADIX == 8 {
        parse_integer_from_oct(s)
    } else {
        unreachable!()
    }
}

fn parse_integer_from_hex(s: &str) -> f64 {
    debug_assert!(s.chars().all(|c| c.is_ascii_hexdigit()));
    const MAX_FAST_INT_LEN: usize = MAX_SAFE_INT.ilog(16) as usize;
    if s.len() > MAX_FAST_INT_LEN {
        // Hex digit character representations:
        //   b'0'==0x30, b'1'==0x31 ... b'9'==0x39  → low nibble: 0x0-0x9
        //   b'A'==0x41, b'B'==0x42 ... b'F'==0x46  → low nibble: 0x1-0x6
        //   b'a'==0x61, b'b'==0x62 ... b'f'==0x66  → low nibble: 0x1-0x6
        //
        // Conversion requires only the low 4 bits:
        //   digit_char & 0x0F gives base value:
        //     - For '0'-'9': direct value (0-9)
        //     - For 'A'-'F'/'a'-'f': offset base (1-6)
        //   Add 9 for alphabetic chars: (low_nibble + 9) → 0xA-0xF
        //
        // Example: (b'A' & 0x0f) + 9 == 0x1 + 9 == 0xA
        s.as_bytes().iter().fold(0f64, |res, &cur| {
            res.mul_add(
                16.,
                if cur < b'A' {
                    cur & 0xf
                } else {
                    (cur & 0xf) + 9
                } as f64,
            )
        })
    } else {
        u64::from_str_radix(s, 16).unwrap() as f64
    }
}

fn parse_integer_from_bin(s: &str) -> f64 {
    debug_assert!(s.chars().all(|c| c == '0' || c == '1'));
    const MAX_FAST_INT_LEN: usize = MAX_SAFE_INT.ilog2() as usize;
    if s.len() > MAX_FAST_INT_LEN {
        s.as_bytes().iter().fold(0f64, |res, &cur| {
            res.mul_add(2., if cur == b'0' { 0. } else { 1. })
        })
    } else {
        u64::from_str_radix(s, 2).unwrap() as f64
    }
}

fn parse_integer_from_oct(s: &str) -> f64 {
    debug_assert!(s.chars().all(|c| matches!(c, '0'..='7')));
    const MAX_FAST_INT_LEN: usize = MAX_SAFE_INT.ilog(8) as usize;
    if s.len() > MAX_FAST_INT_LEN {
        s.as_bytes()
            .iter()
            .fold(0f64, |res, &cur| res.mul_add(8., (cur - b'0') as f64))
    } else {
        u64::from_str_radix(s, 8).unwrap() as f64
    }
}

fn parse_integer_from_dec(s: &str) -> f64 {
    debug_assert!(s.chars().all(|c| c.is_ascii_digit()));
    const MAX_FAST_INT_LEN: usize = MAX_SAFE_INT.ilog10() as usize;
    if s.len() > MAX_FAST_INT_LEN {
        s.parse::<f64>().unwrap()
    } else {
        s.parse::<u64>().unwrap() as f64
    }
}
