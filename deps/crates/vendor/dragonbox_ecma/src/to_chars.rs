// Translated from C++ to Rust. The original C++ code can be found at
// https://github.com/jk-jeon/dragonbox and carries the following license:
//
// Copyright 2020-2025 Junekey Jeon
//
// The contents of this file may be used under the terms of
// the Apache License v2.0 with LLVM Exceptions.
//
//    (See accompanying file LICENSE-Apache or copy at
//     https://llvm.org/foundation/relicensing/LICENSE.txt)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

use core::ptr;

// sign(1) + significand + decimal_point(1) + exp_marker(1) + exp_sign(1) + exp
pub(crate) const MAX_OUTPUT_STRING_LENGTH: usize =
    1 + crate::DECIMAL_SIGNIFICAND_DIGITS + 1 + 1 + 1 + crate::DECIMAL_EXPONENT_DIGITS;

pub(crate) unsafe fn to_chars(x: f64, mut buffer: *mut u8) -> *mut u8 {
    let br = x.to_bits();
    let s = crate::remove_exponent_bits(br);

    if crate::is_negative(s) {
        *buffer = b'-';
        buffer = buffer.add(1);
    }

    if crate::is_nonzero(br) {
        let result = crate::to_decimal(x);
        to_chars_detail(result.significand, result.exponent, buffer)
    } else {
        *buffer = b'0';
        *buffer.add(1) = b'E';
        *buffer.add(2) = b'0';
        buffer.add(3)
    }
}

type BytePair = [u8; 2];

#[rustfmt::skip]
static RADIX_100_TABLE: [BytePair; 100] = [
    [b'0', b'0'], [b'0', b'1'], [b'0', b'2'], [b'0', b'3'], [b'0', b'4'],
    [b'0', b'5'], [b'0', b'6'], [b'0', b'7'], [b'0', b'8'], [b'0', b'9'],
    [b'1', b'0'], [b'1', b'1'], [b'1', b'2'], [b'1', b'3'], [b'1', b'4'],
    [b'1', b'5'], [b'1', b'6'], [b'1', b'7'], [b'1', b'8'], [b'1', b'9'],
    [b'2', b'0'], [b'2', b'1'], [b'2', b'2'], [b'2', b'3'], [b'2', b'4'],
    [b'2', b'5'], [b'2', b'6'], [b'2', b'7'], [b'2', b'8'], [b'2', b'9'],
    [b'3', b'0'], [b'3', b'1'], [b'3', b'2'], [b'3', b'3'], [b'3', b'4'],
    [b'3', b'5'], [b'3', b'6'], [b'3', b'7'], [b'3', b'8'], [b'3', b'9'],
    [b'4', b'0'], [b'4', b'1'], [b'4', b'2'], [b'4', b'3'], [b'4', b'4'],
    [b'4', b'5'], [b'4', b'6'], [b'4', b'7'], [b'4', b'8'], [b'4', b'9'],
    [b'5', b'0'], [b'5', b'1'], [b'5', b'2'], [b'5', b'3'], [b'5', b'4'],
    [b'5', b'5'], [b'5', b'6'], [b'5', b'7'], [b'5', b'8'], [b'5', b'9'],
    [b'6', b'0'], [b'6', b'1'], [b'6', b'2'], [b'6', b'3'], [b'6', b'4'],
    [b'6', b'5'], [b'6', b'6'], [b'6', b'7'], [b'6', b'8'], [b'6', b'9'],
    [b'7', b'0'], [b'7', b'1'], [b'7', b'2'], [b'7', b'3'], [b'7', b'4'],
    [b'7', b'5'], [b'7', b'6'], [b'7', b'7'], [b'7', b'8'], [b'7', b'9'],
    [b'8', b'0'], [b'8', b'1'], [b'8', b'2'], [b'8', b'3'], [b'8', b'4'],
    [b'8', b'5'], [b'8', b'6'], [b'8', b'7'], [b'8', b'8'], [b'8', b'9'],
    [b'9', b'0'], [b'9', b'1'], [b'9', b'2'], [b'9', b'3'], [b'9', b'4'],
    [b'9', b'5'], [b'9', b'6'], [b'9', b'7'], [b'9', b'8'], [b'9', b'9'],
];

#[rustfmt::skip]
static RADIX_100_HEAD_TABLE: [BytePair; 100] = [
    [b'0', b'.'], [b'1', b'.'], [b'2', b'.'], [b'3', b'.'], [b'4', b'.'],
    [b'5', b'.'], [b'6', b'.'], [b'7', b'.'], [b'8', b'.'], [b'9', b'.'],
    [b'1', b'.'], [b'1', b'.'], [b'1', b'.'], [b'1', b'.'], [b'1', b'.'],
    [b'1', b'.'], [b'1', b'.'], [b'1', b'.'], [b'1', b'.'], [b'1', b'.'],
    [b'2', b'.'], [b'2', b'.'], [b'2', b'.'], [b'2', b'.'], [b'2', b'.'],
    [b'2', b'.'], [b'2', b'.'], [b'2', b'.'], [b'2', b'.'], [b'2', b'.'],
    [b'3', b'.'], [b'3', b'.'], [b'3', b'.'], [b'3', b'.'], [b'3', b'.'],
    [b'3', b'.'], [b'3', b'.'], [b'3', b'.'], [b'3', b'.'], [b'3', b'.'],
    [b'4', b'.'], [b'4', b'.'], [b'4', b'.'], [b'4', b'.'], [b'4', b'.'],
    [b'4', b'.'], [b'4', b'.'], [b'4', b'.'], [b'4', b'.'], [b'4', b'.'],
    [b'5', b'.'], [b'5', b'.'], [b'5', b'.'], [b'5', b'.'], [b'5', b'.'],
    [b'5', b'.'], [b'5', b'.'], [b'5', b'.'], [b'5', b'.'], [b'5', b'.'],
    [b'6', b'.'], [b'6', b'.'], [b'6', b'.'], [b'6', b'.'], [b'6', b'.'],
    [b'6', b'.'], [b'6', b'.'], [b'6', b'.'], [b'6', b'.'], [b'6', b'.'],
    [b'7', b'.'], [b'7', b'.'], [b'7', b'.'], [b'7', b'.'], [b'7', b'.'],
    [b'7', b'.'], [b'7', b'.'], [b'7', b'.'], [b'7', b'.'], [b'7', b'.'],
    [b'8', b'.'], [b'8', b'.'], [b'8', b'.'], [b'8', b'.'], [b'8', b'.'],
    [b'8', b'.'], [b'8', b'.'], [b'8', b'.'], [b'8', b'.'], [b'8', b'.'],
    [b'9', b'.'], [b'9', b'.'], [b'9', b'.'], [b'9', b'.'], [b'9', b'.'],
    [b'9', b'.'], [b'9', b'.'], [b'9', b'.'], [b'9', b'.'], [b'9', b'.'],
];

unsafe fn print_1_digit(n: u32, buffer: *mut u8) {
    const {
        assert!((b'0' & 0xf) == 0);
    }
    *buffer = b'0' | n as u8;
}

unsafe fn print_2_digits(n: u32, buffer: *mut u8) {
    let bp = *RADIX_100_TABLE.get_unchecked(n as usize);
    *buffer.cast::<BytePair>() = bp;
}

unsafe fn print_head_chars(n: u32, buffer: *mut u8) {
    let bp = *RADIX_100_HEAD_TABLE.get_unchecked(n as usize);
    *buffer.cast::<BytePair>() = bp;
}

// These digit generation routines are inspired by James Anhalt's itoa
// algorithm: https://github.com/jeaiii/itoa
// The main idea is for given n, find y such that floor(10^k * y / 2^32) = n
// holds, where k is an appropriate integer depending on the length of n. For
// example, if n = 1234567, we set k = 6. In this case, we have
// floor(y / 2^32) = 1,
// floor(10^2 * ((10^0 * y) mod 2^32) / 2^32) = 23,
// floor(10^2 * ((10^2 * y) mod 2^32) / 2^32) = 45, and
// floor(10^2 * ((10^4 * y) mod 2^32) / 2^32) = 67.
// See https://jk-jeon.github.io/posts/2022/02/jeaiii-algorithm/ for more explanation.

unsafe fn print_9_digits(s32: u32, exponent: &mut i32, buffer: &mut *mut u8) {
    // -- IEEE-754 binary32
    // Since we do not cut trailing zeros in advance, s32 must be of 6~9 digits
    // unless the original input was subnormal. In particular, when it is of 9
    // digits it shouldn't have any trailing zeros.
    // -- IEEE-754 binary64
    // In this case, s32 must be of 7~9 digits unless the input is subnormal,
    // and it shouldn't have any trailing zeros if it is of 9 digits.
    if s32 >= 1_0000_0000 {
        // 9 digits.
        // 1441151882 = ceil(2^57 / 1_0000_0000) + 1
        let mut prod = u64::from(s32) * 1441151882;
        prod >>= 25;
        print_head_chars((prod >> 32) as u32, *buffer);

        prod = (prod & 0xffffffff) * 100;
        print_2_digits((prod >> 32) as u32, buffer.add(2));
        prod = (prod & 0xffffffff) * 100;
        print_2_digits((prod >> 32) as u32, buffer.add(4));
        prod = (prod & 0xffffffff) * 100;
        print_2_digits((prod >> 32) as u32, buffer.add(6));
        prod = (prod & 0xffffffff) * 100;
        print_2_digits((prod >> 32) as u32, buffer.add(8));

        *exponent += 8;
        *buffer = buffer.add(10);
    } else if s32 >= 100_0000 {
        // 7 or 8 digits.
        // 281474978 = ceil(2^48 / 100_0000) + 1
        let mut prod = u64::from(s32) * 281474978;
        prod >>= 16;
        let head_digits = (prod >> 32) as u32;
        // If s32 is of 8 digits, increase the exponent by 7.
        // Otherwise, increase it by 6.
        *exponent += 6 + i32::from(head_digits >= 10);

        // Write the first digit and the decimal point.
        print_head_chars(head_digits, *buffer);
        // This third character may be overwritten later but we don't care.
        *buffer.add(2) = RADIX_100_TABLE.get_unchecked(head_digits as usize)[1];

        // Remaining 6 digits are all zero?
        if prod as u32 <= ((1u64 << 32) / 100_0000) as u32 {
            // The number of characters actually need to be written is:
            //   1, if only the first digit is nonzero, which means that either s32 is of 7
            //   digits or it is of 8 digits but the second digit is zero, or
            //   3, otherwise.
            // Note that buffer[2] is never '0' if s32 is of 7 digits, because the input is
            // never zero.
            *buffer = buffer
                .add(1 + (usize::from(head_digits >= 10) & usize::from(*buffer.add(2) > b'0')) * 2);
        } else {
            // At least one of the remaining 6 digits are nonzero.
            // After this adjustment, now the first destination becomes buffer + 2.
            *buffer = buffer.add(usize::from(head_digits >= 10));

            // Obtain the next two digits.
            prod = (prod & 0xffffffff) * 100;
            print_2_digits((prod >> 32) as u32, buffer.add(2));

            // Remaining 4 digits are all zero?
            if prod as u32 <= ((1u64 << 32) / 1_0000) as u32 {
                *buffer = buffer.add(3 + usize::from(*buffer.add(3) > b'0'));
            } else {
                // At least one of the remaining 4 digits are nonzero.

                // Obtain the next two digits.
                prod = (prod & 0xffffffff) * 100;
                print_2_digits((prod >> 32) as u32, buffer.add(4));

                // Remaining 2 digits are all zero?
                if prod as u32 <= ((1u64 << 32) / 100) as u32 {
                    *buffer = buffer.add(5 + usize::from(*buffer.add(5) > b'0'));
                } else {
                    // Obtain the last two digits.
                    prod = (prod & 0xffffffff) * 100;
                    print_2_digits((prod >> 32) as u32, buffer.add(6));

                    *buffer = buffer.add(7 + usize::from(*buffer.add(7) > b'0'));
                }
            }
        }
    } else if s32 >= 1_0000 {
        // 5 or 6 digits.
        // 429497 = ceil(2^32 / 1_0000)
        let mut prod = u64::from(s32) * 429497;
        let head_digits = (prod >> 32) as u32;

        // If s32 is of 6 digits, increase the exponent by 5.
        // Otherwise, increase it by 4.
        *exponent += 4 + i32::from(head_digits >= 10);

        // Write the first digit and the decimal point.
        print_head_chars(head_digits, *buffer);
        // This third character may be overwritten later but we don't care.
        *buffer.add(2) = RADIX_100_TABLE.get_unchecked(head_digits as usize)[1];

        // Remaining 4 digits are all zero?
        if prod as u32 <= ((1u64 << 32) / 1_0000) as u32 {
            // The number of characters actually written is 1 or 3, similarly to the case of
            // 7 or 8 digits.
            *buffer = buffer
                .add(1 + (usize::from(head_digits >= 10) & usize::from(*buffer.add(2) > b'0')) * 2);
        } else {
            // At least one of the remaining 4 digits are nonzero.
            // After this adjustment, now the first destination becomes buffer + 2.
            *buffer = buffer.add(usize::from(head_digits >= 10));

            // Obtain the next two digits.
            prod = (prod & 0xffffffff) * 100;
            print_2_digits((prod >> 32) as u32, buffer.add(2));

            // Remaining 2 digits are all zero?
            if prod as u32 <= ((1u64 << 32) / 100) as u32 {
                *buffer = buffer.add(3 + usize::from(*buffer.add(3) > b'0'));
            } else {
                // Obtain the last two digits.
                prod = (prod & 0xffffffff) * 100;
                print_2_digits((prod >> 32) as u32, buffer.add(4));

                *buffer = buffer.add(5 + usize::from(*buffer.add(5) > b'0'));
            }
        }
    } else if s32 >= 100 {
        // 3 or 4 digits.
        // 42949673 = ceil(2^32 / 100)
        let mut prod = u64::from(s32) * 42949673;
        let head_digits = (prod >> 32) as u32;

        // If s32 is of 4 digits, increase the exponent by 3.
        // Otherwise, increase it by 2.
        *exponent += 2 + i32::from(head_digits >= 10);

        // Write the first digit and the decimal point.
        print_head_chars(head_digits, *buffer);
        // This third character may be overwritten later but we don't care.
        *buffer.add(2) = RADIX_100_TABLE.get_unchecked(head_digits as usize)[1];

        // Remaining 2 digits are all zero?
        if prod as u32 <= ((1u64 << 32) / 100) as u32 {
            // The number of characters actually written is 1 or 3, similarly to the case of
            // 7 or 8 digits.
            *buffer = buffer
                .add(1 + (usize::from(head_digits >= 10) & usize::from(*buffer.add(2) > b'0')) * 2);
        } else {
            // At least one of the remaining 2 digits are nonzero.
            // After this adjustment, now the first destination becomes buffer + 2.
            *buffer = buffer.add(usize::from(head_digits >= 10));

            // Obtain the last two digits.
            prod = (prod & 0xffffffff) * 100;
            print_2_digits((prod >> 32) as u32, buffer.add(2));

            *buffer = buffer.add(3 + usize::from(*buffer.add(3) > b'0'));
        }
    } else {
        // 1 or 2 digits.
        // If s32 is of 2 digits, increase the exponent by 1.
        *exponent += i32::from(s32 >= 10);

        // Write the first digit and the decimal point.
        print_head_chars(s32, *buffer);
        // This third character may be overwritten later but we don't care.
        *buffer.add(2) = RADIX_100_TABLE.get_unchecked(s32 as usize)[1];

        // The number of characters actually written is 1 or 3, similarly to the case of
        // 7 or 8 digits.
        *buffer = buffer.add(1 + (usize::from(s32 >= 10) & usize::from(*buffer.add(2) > b'0')) * 2);
    }
}

unsafe fn to_chars_detail(significand: u64, mut exponent: i32, mut buffer: *mut u8) -> *mut u8 {
    // Print significand by decomposing it into a 9-digit block and a 8-digit block.
    let first_block: u32;
    let second_block: u32;
    let no_second_block: bool;

    if significand >= 1_0000_0000 {
        first_block = (significand / 1_0000_0000) as u32;
        second_block = (significand as u32).wrapping_sub(first_block.wrapping_mul(1_0000_0000));
        exponent += 8;
        no_second_block = second_block == 0;
    } else {
        first_block = significand as u32;
        second_block = 0;
        no_second_block = true;
    }

    if no_second_block {
        print_9_digits(first_block, &mut exponent, &mut buffer);
    } else {
        // We proceed similarly to print_9_digits(), but since we do not need to remove
        // trailing zeros, the procedure is a bit simpler.
        if first_block >= 1_0000_0000 {
            // The input is of 17 digits, thus there should be no trailing zero at all.
            // The first block is of 9 digits.
            // 1441151882 = ceil(2^57 / 1_0000_0000) + 1
            let mut prod = u64::from(first_block) * 1441151882;
            prod >>= 25;
            print_head_chars((prod >> 32) as u32, buffer);

            prod = (prod & 0xffffffff) * 100;
            print_2_digits((prod >> 32) as u32, buffer.add(2));
            prod = (prod & 0xffffffff) * 100;
            print_2_digits((prod >> 32) as u32, buffer.add(4));
            prod = (prod & 0xffffffff) * 100;
            print_2_digits((prod >> 32) as u32, buffer.add(6));
            prod = (prod & 0xffffffff) * 100;
            print_2_digits((prod >> 32) as u32, buffer.add(8));

            // The second block is of 8 digits.
            // 281474978 = ceil(2^48 / 100'0000) + 1
            prod = u64::from(second_block) * 281474978;
            prod >>= 16;
            prod += 1;
            print_2_digits((prod >> 32) as u32, buffer.add(10));
            prod = (prod & 0xffffffff) * 100;
            print_2_digits((prod >> 32) as u32, buffer.add(12));
            prod = (prod & 0xffffffff) * 100;
            print_2_digits((prod >> 32) as u32, buffer.add(14));
            prod = (prod & 0xffffffff) * 100;
            print_2_digits((prod >> 32) as u32, buffer.add(16));

            exponent += 8;
            buffer = buffer.add(18);
        } else {
            if first_block >= 100_0000 {
                // 7 or 8 digits.
                // 281474978 = ceil(2^48 / 100_0000) + 1
                let mut prod = u64::from(first_block) * 281474978;
                prod >>= 16;
                let head_digits = (prod >> 32) as u32;

                print_head_chars(head_digits, buffer);
                *buffer.add(2) = RADIX_100_TABLE.get_unchecked(head_digits as usize)[1];

                exponent += 6 + i32::from(head_digits >= 10);
                buffer = buffer.add(usize::from(head_digits >= 10));

                // Print remaining 6 digits.
                prod = (prod & 0xffffffff) * 100;
                print_2_digits((prod >> 32) as u32, buffer.add(2));
                prod = (prod & 0xffffffff) * 100;
                print_2_digits((prod >> 32) as u32, buffer.add(4));
                prod = (prod & 0xffffffff) * 100;
                print_2_digits((prod >> 32) as u32, buffer.add(6));

                buffer = buffer.add(8);
            } else if first_block >= 1_0000 {
                // 5 or 6 digits.
                // 429497 = ceil(2^32 / 1_0000)
                let mut prod = u64::from(first_block) * 429497;
                let head_digits = (prod >> 32) as u32;

                print_head_chars(head_digits, buffer);
                *buffer.add(2) = RADIX_100_TABLE.get_unchecked(head_digits as usize)[1];

                exponent += 4 + i32::from(head_digits >= 10);
                buffer = buffer.add(usize::from(head_digits >= 10));

                // Print remaining 4 digits.
                prod = (prod & 0xffffffff) * 100;
                print_2_digits((prod >> 32) as u32, buffer.add(2));
                prod = (prod & 0xffffffff) * 100;
                print_2_digits((prod >> 32) as u32, buffer.add(4));

                buffer = buffer.add(6);
            } else if first_block >= 100 {
                // 3 or 4 digits.
                // 42949673 = ceil(2^32 / 100)
                let mut prod = u64::from(first_block) * 42949673;
                let head_digits = (prod >> 32) as u32;

                print_head_chars(head_digits, buffer);
                *buffer.add(2) = RADIX_100_TABLE.get_unchecked(head_digits as usize)[1];

                exponent += 2 + i32::from(head_digits >= 10);
                buffer = buffer.add(usize::from(head_digits >= 10));

                // Print remaining 2 digits.
                prod = (prod & 0xffffffff) * 100;
                print_2_digits((prod >> 32) as u32, buffer.add(2));

                buffer = buffer.add(4);
            } else {
                // 1 or 2 digits.
                print_head_chars(first_block, buffer);
                *buffer.add(2) = RADIX_100_TABLE.get_unchecked(first_block as usize)[1];

                exponent += i32::from(first_block >= 10);
                buffer = buffer.add(2 + usize::from(first_block >= 10));
            }

            // Next, print the second block.
            // The second block is of 8 digits, but we may have trailing zeros.
            // 281474978 = ceil(2^48 / 100_0000) + 1
            let mut prod = u64::from(second_block) * 281474978;
            prod >>= 16;
            prod += 1;
            print_2_digits((prod >> 32) as u32, buffer);

            // Remaining 6 digits are all zero?
            if prod as u32 <= ((1u64 << 32) / 100_0000) as u32 {
                buffer = buffer.add(1 + usize::from(*buffer.add(1) > b'0'));
            } else {
                // Obtain the next two digits.
                prod = (prod & 0xffffffff) * 100;
                print_2_digits((prod >> 32) as u32, buffer.add(2));

                // Remaining 4 digits are all zero?
                if prod as u32 <= ((1u64 << 32) / 1_0000) as u32 {
                    buffer = buffer.add(3 + usize::from(*buffer.add(3) > b'0'));
                } else {
                    // Obtain the next two digits.
                    prod = (prod & 0xffffffff) * 100;
                    print_2_digits((prod >> 32) as u32, buffer.add(4));

                    // Remaining 2 digits are all zero?
                    if prod as u32 <= ((1u64 << 32) / 100) as u32 {
                        buffer = buffer.add(5 + usize::from(*buffer.add(5) > b'0'));
                    } else {
                        // Obtain the last two digits.
                        prod = (prod & 0xffffffff) * 100;
                        print_2_digits((prod >> 32) as u32, buffer.add(6));
                        buffer = buffer.add(7 + usize::from(*buffer.add(7) > b'0'));
                    }
                }
            }
        }
    }

    // Print exponent and return
    if exponent < 0 {
        ptr::copy_nonoverlapping(b"E-".as_ptr().cast::<u8>(), buffer, 2);
        buffer = buffer.add(2);
        exponent = -exponent;
    } else {
        *buffer = b'E';
        buffer = buffer.add(1);
    }

    if exponent >= 100 {
        // d1 = exponent / 10; d2 = exponent % 10;
        // 6554 = ceil(2^16 / 10)
        let d1 = (exponent as u32 * 6554) >> 16;
        let d2 = exponent as u32 - 10 * d1;
        print_2_digits(d1, buffer);
        print_1_digit(d2, buffer.add(2));
        buffer = buffer.add(3);
    } else if exponent >= 10 {
        print_2_digits(exponent as u32, buffer);
        buffer = buffer.add(2);
    } else {
        print_1_digit(exponent as u32, buffer);
        buffer = buffer.add(1);
    }

    buffer
}
