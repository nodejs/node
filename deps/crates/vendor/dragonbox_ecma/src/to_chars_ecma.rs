// https://chromium-review.googlesource.com/c/v8/v8/+/6494437
// Ported from V8's `DoubleToStringView` ECMA-262 compliant implementation
// This provides JavaScript Number.toString() behavior with dragonbox performance

use core::ptr;

// Maximum length of output string for any f64 number
// https://viewer.scuttlebot.io/%25LQo5KOMeR%2Baj%2BEj0JVg3qLRqr%2BwiKo74nS8Uz7o0LDM%3D.sha256
// -0.0000015809161985788154
pub(crate) const MAX_OUTPUT_STRING_LENGTH: usize = 25;

// Lookup tables for fast digit conversion (direct port from V8)
#[rustfmt::skip]
static RADIX_100_TABLE: [u8; 200] = [
    b'0', b'0', b'0', b'1', b'0', b'2', b'0', b'3', b'0', b'4',
    b'0', b'5', b'0', b'6', b'0', b'7', b'0', b'8', b'0', b'9',
    b'1', b'0', b'1', b'1', b'1', b'2', b'1', b'3', b'1', b'4',
    b'1', b'5', b'1', b'6', b'1', b'7', b'1', b'8', b'1', b'9',
    b'2', b'0', b'2', b'1', b'2', b'2', b'2', b'3', b'2', b'4',
    b'2', b'5', b'2', b'6', b'2', b'7', b'2', b'8', b'2', b'9',
    b'3', b'0', b'3', b'1', b'3', b'2', b'3', b'3', b'3', b'4',
    b'3', b'5', b'3', b'6', b'3', b'7', b'3', b'8', b'3', b'9',
    b'4', b'0', b'4', b'1', b'4', b'2', b'4', b'3', b'4', b'4',
    b'4', b'5', b'4', b'6', b'4', b'7', b'4', b'8', b'4', b'9',
    b'5', b'0', b'5', b'1', b'5', b'2', b'5', b'3', b'5', b'4',
    b'5', b'5', b'5', b'6', b'5', b'7', b'5', b'8', b'5', b'9',
    b'6', b'0', b'6', b'1', b'6', b'2', b'6', b'3', b'6', b'4',
    b'6', b'5', b'6', b'6', b'6', b'7', b'6', b'8', b'6', b'9',
    b'7', b'0', b'7', b'1', b'7', b'2', b'7', b'3', b'7', b'4',
    b'7', b'5', b'7', b'6', b'7', b'7', b'7', b'8', b'7', b'9',
    b'8', b'0', b'8', b'1', b'8', b'2', b'8', b'3', b'8', b'4',
    b'8', b'5', b'8', b'6', b'8', b'7', b'8', b'8', b'8', b'9',
    b'9', b'0', b'9', b'1', b'9', b'2', b'9', b'3', b'9', b'4',
    b'9', b'5', b'9', b'6', b'9', b'7', b'9', b'8', b'9', b'9',
];

#[rustfmt::skip]
static RADIX_100_HEAD_TABLE: [u8; 200] = [
    0x00, 0x00, b'1', 0x00, b'2', 0x00, b'3', 0x00, b'4', 0x00,
    b'5', 0x00, b'6', 0x00, b'7', 0x00, b'8', 0x00, b'9', 0x00,
    b'1', b'0', b'1', b'1', b'1', b'2', b'1', b'3', b'1', b'4',
    b'1', b'5', b'1', b'6', b'1', b'7', b'1', b'8', b'1', b'9',
    b'2', b'0', b'2', b'1', b'2', b'2', b'2', b'3', b'2', b'4',
    b'2', b'5', b'2', b'6', b'2', b'7', b'2', b'8', b'2', b'9',
    b'3', b'0', b'3', b'1', b'3', b'2', b'3', b'3', b'3', b'4',
    b'3', b'5', b'3', b'6', b'3', b'7', b'3', b'8', b'3', b'9',
    b'4', b'0', b'4', b'1', b'4', b'2', b'4', b'3', b'4', b'4',
    b'4', b'5', b'4', b'6', b'4', b'7', b'4', b'8', b'4', b'9',
    b'5', b'0', b'5', b'1', b'5', b'2', b'5', b'3', b'5', b'4',
    b'5', b'5', b'5', b'6', b'5', b'7', b'5', b'8', b'5', b'9',
    b'6', b'0', b'6', b'1', b'6', b'2', b'6', b'3', b'6', b'4',
    b'6', b'5', b'6', b'6', b'6', b'7', b'6', b'8', b'6', b'9',
    b'7', b'0', b'7', b'1', b'7', b'2', b'7', b'3', b'7', b'4',
    b'7', b'5', b'7', b'6', b'7', b'7', b'7', b'8', b'7', b'9',
    b'8', b'0', b'8', b'1', b'8', b'2', b'8', b'3', b'8', b'4',
    b'8', b'5', b'8', b'6', b'8', b'7', b'8', b'8', b'8', b'9',
    b'9', b'0', b'9', b'1', b'9', b'2', b'9', b'3', b'9', b'4',
    b'9', b'5', b'9', b'6', b'9', b'7', b'9', b'8', b'9', b'9',
];

// Main entry point for number to string conversion
pub(crate) unsafe fn to_chars(x: f64, mut buffer: *mut u8) -> *mut u8 {
    let br = x.to_bits();
    let s = crate::remove_exponent_bits(br);

    if crate::is_nonzero(br) {
        if crate::is_negative(s) {
            *buffer = b'-';
            buffer = buffer.add(1);
        }

        let result = crate::to_decimal(x);
        to_chars_detail(result.significand, result.exponent, buffer)
    } else {
        *buffer = b'0';
        buffer.add(1)
    }
}

// V8's Convert2Digits function
unsafe fn convert_2_digits(n: u8, buffer: *mut u8) {
    debug_assert!(n < 100);
    let index = (n as usize) * 2;
    ptr::copy_nonoverlapping(RADIX_100_TABLE.as_ptr().add(index), buffer, 2);
}

// V8's ConvertHeadDigits function - returns digit count
unsafe fn convert_head_digits(n: u8, buffer: *mut u8) -> u8 {
    debug_assert!(n < 100);
    let digit_count = 1 + u8::from(n >= 10);
    let index = (n as usize) * 2;
    ptr::copy_nonoverlapping(
        RADIX_100_HEAD_TABLE.as_ptr().add(index),
        buffer,
        digit_count as usize,
    );
    digit_count
}

// V8's Convert8Digits function
unsafe fn convert_8_digits(n: u32, buffer: *mut u8) {
    const K_UINT32_MASK: u64 = u32::MAX as u64;

    // 281474978 = ceil(2^48 / 1'000'000) + 1
    let mut prod = u64::from(n) * 281474978;
    prod >>= 16;
    prod += 1;
    convert_2_digits((prod >> 32) as u8, buffer);
    prod = (prod & K_UINT32_MASK) * 100;
    convert_2_digits((prod >> 32) as u8, buffer.add(2));
    prod = (prod & K_UINT32_MASK) * 100;
    convert_2_digits((prod >> 32) as u8, buffer.add(4));
    prod = (prod & K_UINT32_MASK) * 100;
    convert_2_digits((prod >> 32) as u8, buffer.add(6));
}

// V8's ConvertUpTo9Digits function - returns digit count
unsafe fn convert_up_to_9_digits(n: u32, mut buffer: *mut u8) -> u8 {
    const K_UINT32_MASK: u64 = u32::MAX as u64;

    if n >= 100_000_000 {
        // 9 digits.
        // 1441151882 = ceil(2^57 / 100'000'000) + 1
        let mut prod = u64::from(n) * 1441151882;
        prod >>= 25;

        let head_digit = (prod >> 32) as u8;
        debug_assert!(head_digit < 10);
        *buffer = b'0' + head_digit;

        // Print remaining 8 digits.
        prod = (prod & K_UINT32_MASK) * 100;
        convert_2_digits((prod >> 32) as u8, buffer.add(1));
        prod = (prod & K_UINT32_MASK) * 100;
        convert_2_digits((prod >> 32) as u8, buffer.add(3));
        prod = (prod & K_UINT32_MASK) * 100;
        convert_2_digits((prod >> 32) as u8, buffer.add(5));
        prod = (prod & K_UINT32_MASK) * 100;
        convert_2_digits((prod >> 32) as u8, buffer.add(7));

        return 9;
    }
    if n >= 1_000_000 {
        // 7 or 8 digits.
        // 281474978 = ceil(2^48 / 1'000'000) + 1
        let mut prod = u64::from(n) * 281474978;
        prod >>= 16;

        let head_digits = (prod >> 32) as u8;
        let head_digit_count = convert_head_digits(head_digits, buffer);
        buffer = buffer.add(head_digit_count as usize);

        // Print remaining 6 digits.
        prod = (prod & K_UINT32_MASK) * 100;
        convert_2_digits((prod >> 32) as u8, buffer);
        prod = (prod & K_UINT32_MASK) * 100;
        convert_2_digits((prod >> 32) as u8, buffer.add(2));
        prod = (prod & K_UINT32_MASK) * 100;
        convert_2_digits((prod >> 32) as u8, buffer.add(4));

        return 6 + head_digit_count;
    }
    if n >= 10_000 {
        // 5 or 6 digits.
        // 429497 = ceil(2^32 / 10'000)
        let mut prod = u64::from(n) * 429497;

        let head_digits = (prod >> 32) as u8;
        let head_digit_count = convert_head_digits(head_digits, buffer);
        buffer = buffer.add(head_digit_count as usize);

        // Print remaining 4 digits.
        prod = (prod & K_UINT32_MASK) * 100;
        convert_2_digits((prod >> 32) as u8, buffer);
        prod = (prod & K_UINT32_MASK) * 100;
        convert_2_digits((prod >> 32) as u8, buffer.add(2));

        return 4 + head_digit_count;
    }
    if n >= 100 {
        // 3 or 4 digits.
        // 42949673 = ceil(2^32 / 100)
        let mut prod = u64::from(n) * 42949673;

        let head_digits = (prod >> 32) as u8;
        let head_digit_count = convert_head_digits(head_digits, buffer);
        buffer = buffer.add(head_digit_count as usize);

        // Print remaining 2 digits.
        prod = (prod & K_UINT32_MASK) * 100;
        convert_2_digits((prod >> 32) as u8, buffer);

        return 2 + head_digit_count;
    }
    // 1 or 2 digits.
    convert_head_digits(n as u8, buffer)
}

// V8's SignificandToChars function - returns digit count
unsafe fn significand_to_chars(n: u64, buffer: *mut u8) -> u8 {
    debug_assert!(n < 99999999999999999); // Only supports up to 17 digits

    if n >= 100_000_000 {
        // If we have at least 9 digits, split into 2 blocks. The second one always
        // has exactly 8 digits.
        let first_block = (n / 100_000_000) as u32;
        let second_block = (n % 100_000_000) as u32;

        let first_block_digits = convert_up_to_9_digits(first_block, buffer);
        convert_8_digits(second_block, buffer.add(first_block_digits as usize));
        first_block_digits + 8
    } else {
        convert_up_to_9_digits(n as u32, buffer)
    }
}

// Add exponent helper - ported from V8's more efficient implementation
unsafe fn add_exponent(exp: i32, mut buffer: *mut u8) -> *mut u8 {
    debug_assert!(exp >= 0);
    debug_assert!(exp <= 999);

    if exp >= 100 {
        // d1 = exp / 10; d2 = exp % 10;
        // 6554 = ceil(2^16 / 10)
        let d1 = ((exp as u32) * 6554) >> 16;
        let d2 = (exp as u32) - 10 * d1;
        convert_2_digits(d1 as u8, buffer);
        buffer = buffer.add(2);
        *buffer = b'0' + d2 as u8;
        buffer = buffer.add(1);
    } else if exp >= 10 {
        convert_2_digits(exp as u8, buffer);
        buffer = buffer.add(2);
    } else {
        *buffer = b'0' + exp as u8;
        buffer = buffer.add(1);
    }

    buffer
}

// Main formatting function that implements ECMA-262 section 9.8.1
unsafe fn to_chars_detail(significand: u64, exponent: i32, mut buffer: *mut u8) -> *mut u8 {
    // Convert significand to decimal string using V8's optimized functions
    // No need for temporary buffer since we can write directly to the output buffer
    let mut decimal_rep = [0u8; 17];
    let length = i32::from(significand_to_chars(significand, decimal_rep.as_mut_ptr()));

    let decimal_point = length as i32 + exponent;

    if length <= decimal_point && decimal_point <= 21 {
        // ECMA-262 section 9.8.1 step 6.
        ptr::copy_nonoverlapping(decimal_rep.as_ptr(), buffer, length as usize);
        buffer = buffer.add(length as usize);
        // Add padding zeros
        for _ in 0..(decimal_point - length) {
            *buffer = b'0';
            buffer = buffer.add(1);
        }
    } else if 0 < decimal_point && decimal_point <= 21 {
        // ECMA-262 section 9.8.1 step 7.
        ptr::copy_nonoverlapping(decimal_rep.as_ptr(), buffer, decimal_point as usize);
        buffer = buffer.add(decimal_point as usize);
        *buffer = b'.';
        buffer = buffer.add(1);
        let remaining = length - decimal_point;
        ptr::copy_nonoverlapping(
            decimal_rep.as_ptr().add(decimal_point as usize),
            buffer,
            remaining as usize,
        );
        buffer = buffer.add(remaining as usize);
    } else if decimal_point <= 0 && decimal_point > -6 {
        // ECMA-262 section 9.8.1 step 8.
        ptr::copy_nonoverlapping(b"0.".as_ptr(), buffer, 2);
        buffer = buffer.add(2);
        // Add leading zeros
        for _ in 0..(-decimal_point) {
            *buffer = b'0';
            buffer = buffer.add(1);
        }
        ptr::copy_nonoverlapping(decimal_rep.as_ptr(), buffer, length as usize);
        buffer = buffer.add(length as usize);
    } else {
        // ECMA-262 section 9.8.1 step 9 and 10 combined.
        *buffer = decimal_rep[0];
        buffer = buffer.add(1);
        if length != 1 {
            *buffer = b'.';
            buffer = buffer.add(1);
            ptr::copy_nonoverlapping(decimal_rep.as_ptr().add(1), buffer, (length - 1) as usize);
            buffer = buffer.add((length - 1) as usize);
        }
        *buffer = b'e';
        buffer = buffer.add(1);
        let exp = decimal_point - 1;
        if exp >= 0 {
            *buffer = b'+';
        } else {
            *buffer = b'-';
        }
        buffer = buffer.add(1);
        buffer = add_exponent(exp.abs(), buffer);
    }

    buffer
}
