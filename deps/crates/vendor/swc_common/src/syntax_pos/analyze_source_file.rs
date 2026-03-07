// Copyright 2018 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
use unicode_width::UnicodeWidthChar;

use super::*;

/// Finds all newlines, multi-byte characters, and non-narrow characters in a
/// SourceFile.
///
/// This function will use an SSE2 enhanced implementation if hardware support
/// is detected at runtime.
pub fn analyze_source_file(
    src: &str,
    source_file_start_pos: BytePos,
) -> (Vec<BytePos>, Vec<MultiByteChar>, Vec<NonNarrowChar>) {
    let mut lines = vec![source_file_start_pos];
    let mut multi_byte_chars = Vec::new();
    let mut non_narrow_chars = Vec::new();

    // Calls the right implementation, depending on hardware support available.
    analyze_source_file_generic(
        src,
        src.len(),
        source_file_start_pos,
        &mut lines,
        &mut multi_byte_chars,
        &mut non_narrow_chars,
    );

    // The code above optimistically registers a new line *after* each \n
    // it encounters. If that point is already outside the source_file, remove
    // it again.
    if let Some(&last_line_start) = lines.last() {
        let source_file_end = source_file_start_pos + BytePos::from_usize(src.len());
        assert!(source_file_end >= last_line_start);
        if last_line_start == source_file_end {
            lines.pop();
        }
    }

    (lines, multi_byte_chars, non_narrow_chars)
}

// `scan_len` determines the number of bytes in `src` to scan. Note that the
// function can read past `scan_len` if a multi-byte character start within the
// range but extends past it. The overflow is returned by the function.
fn analyze_source_file_generic(
    src: &str,
    scan_len: usize,
    output_offset: BytePos,
    lines: &mut Vec<BytePos>,
    multi_byte_chars: &mut Vec<MultiByteChar>,
    non_narrow_chars: &mut Vec<NonNarrowChar>,
) -> usize {
    assert!(src.len() >= scan_len);
    let mut i = 0;
    let src_bytes = src.as_bytes();

    while i < scan_len {
        let byte = unsafe {
            // We verified that i < scan_len <= src.len()
            *src_bytes.get_unchecked(i)
        };

        // How much to advance in order to get to the next UTF-8 char in the
        // string.
        let mut char_len = 1;

        if byte < 32 {
            // This is an ASCII control character, it could be one of the cases
            // that are interesting to us.

            let pos = BytePos::from_usize(i) + output_offset;

            match byte {
                b'\r' => {
                    if let Some(b'\n') = src_bytes.get(i + 1) {
                        lines.push(pos + BytePos(2));
                        i += 2;
                        continue;
                    }
                    lines.push(pos + BytePos(1));
                }

                b'\n' => {
                    lines.push(pos + BytePos(1));
                }
                b'\t' => {
                    non_narrow_chars.push(NonNarrowChar::Tab(pos));
                }
                _ => {
                    non_narrow_chars.push(NonNarrowChar::ZeroWidth(pos));
                }
            }
        } else if byte >= 127 {
            // The slow path:
            // This is either ASCII control character "DEL" or the beginning of
            // a multibyte char. Just decode to `char`.
            let c = src[i..].chars().next().unwrap();
            char_len = c.len_utf8();

            let pos = BytePos::from_usize(i) + output_offset;

            if char_len > 1 {
                assert!((2..=4).contains(&char_len));
                let mbc = MultiByteChar {
                    pos,
                    bytes: char_len as u8,
                };
                multi_byte_chars.push(mbc);
            }

            // Assume control characters are zero width.
            // FIXME: How can we decide between `width` and `width_cjk`?
            let char_width = UnicodeWidthChar::width(c).unwrap_or(0);

            if char_width != 1 {
                non_narrow_chars.push(NonNarrowChar::new(pos, char_width));
            }
        }

        i += char_len;
    }

    i - scan_len
}

#[cfg(test)]
#[allow(clippy::identity_op)]
mod tests {
    use super::*;

    macro_rules! test {
        (case: $test_name:ident,
     text: $text:expr,
     source_file_start_pos: $source_file_start_pos:expr,
     lines: $lines:expr,
     multi_byte_chars: $multi_byte_chars:expr,
     non_narrow_chars: $non_narrow_chars:expr,) => {
            #[test]
            fn $test_name() {
                let (lines, multi_byte_chars, non_narrow_chars) =
                    analyze_source_file($text, BytePos($source_file_start_pos));

                let expected_lines: Vec<BytePos> =
                    $lines.into_iter().map(|pos| BytePos(pos)).collect();

                assert_eq!(lines, expected_lines);

                let expected_mbcs: Vec<MultiByteChar> = $multi_byte_chars
                    .into_iter()
                    .map(|(pos, bytes)| MultiByteChar {
                        pos: BytePos(pos),
                        bytes,
                    })
                    .collect();

                assert_eq!(multi_byte_chars, expected_mbcs);

                let expected_nncs: Vec<NonNarrowChar> = $non_narrow_chars
                    .into_iter()
                    .map(|(pos, width)| NonNarrowChar::new(BytePos(pos), width))
                    .collect();

                assert_eq!(non_narrow_chars, expected_nncs);
            }
        };
    }

    test!(
        case: empty_text,
        text: "",
        source_file_start_pos: 0,
        lines: Vec::new(),
        multi_byte_chars: Vec::new(),
        non_narrow_chars: Vec::new(),
    );

    test!(
        case: newlines_short,
        text: "a\nc",
        source_file_start_pos: 0,
        lines: vec![0, 2],
        multi_byte_chars: Vec::new(),
        non_narrow_chars: Vec::new(),
    );

    test!(
        case: newlines_long,
        text: "012345678\nabcdef012345678\na",
        source_file_start_pos: 0,
        lines: vec![0, 10, 26],
        multi_byte_chars: Vec::new(),
        non_narrow_chars: Vec::new(),
    );

    test!(
        case: newline_and_multi_byte_char_in_same_chunk,
        text: "01234β789\nbcdef0123456789abcdef",
        source_file_start_pos: 0,
        lines: vec![0, 11],
        multi_byte_chars: vec![(5, 2)],
        non_narrow_chars: Vec::new(),
    );

    test!(
        case: newline_and_control_char_in_same_chunk,
        text: "01234\u{07}6789\nbcdef0123456789abcdef",
        source_file_start_pos: 0,
        lines: vec![0, 11],
        multi_byte_chars: Vec::new(),
        non_narrow_chars: vec![(5, 0)],
    );

    test!(
        case: multi_byte_char_short,
        text: "aβc",
        source_file_start_pos: 0,
        lines: vec![0],
        multi_byte_chars: vec![(1, 2)],
        non_narrow_chars: Vec::new(),
    );

    test!(
        case: multi_byte_char_long,
        text: "0123456789abcΔf012345β",
        source_file_start_pos: 0,
        lines: vec![0],
        multi_byte_chars: vec![(13, 2), (22, 2)],
        non_narrow_chars: Vec::new(),
    );

    test!(
        case: multi_byte_char_across_chunk_boundary,
        text: "0123456789abcdeΔ123456789abcdef01234",
        source_file_start_pos: 0,
        lines: vec![0],
        multi_byte_chars: vec![(15, 2)],
        non_narrow_chars: Vec::new(),
    );

    test!(
        case: multi_byte_char_across_chunk_boundary_tail,
        text: "0123456789abcdeΔ....",
        source_file_start_pos: 0,
        lines: vec![0],
        multi_byte_chars: vec![(15, 2)],
        non_narrow_chars: Vec::new(),
    );

    test!(
        case: non_narrow_short,
        text: "0\t2",
        source_file_start_pos: 0,
        lines: vec![0],
        multi_byte_chars: Vec::new(),
        non_narrow_chars: vec![(1, 4)],
    );

    test!(
        case: non_narrow_long,
        text: "01\t3456789abcdef01234567\u{07}9",
        source_file_start_pos: 0,
        lines: vec![0],
        multi_byte_chars: Vec::new(),
        non_narrow_chars: vec![(2, 4), (24, 0)],
    );

    test!(
        case: output_offset_all,
        text: "01\t345\n789abcΔf01234567\u{07}9\nbcΔf",
        source_file_start_pos: 1000,
        lines: vec![0 + 1000, 7 + 1000, 27 + 1000],
        multi_byte_chars: vec![(13 + 1000, 2), (29 + 1000, 2)],
        non_narrow_chars: vec![(2 + 1000, 4), (24 + 1000, 0)],
    );

    test!(
        case: unix_lf,
        text: "/**\n * foo\n */\n012345678\nabcdef012345678\na",
        source_file_start_pos: 0,
        lines: vec![0, 4, 11, 15, 25, 41],
        multi_byte_chars: Vec::new(),
        non_narrow_chars: Vec::new(),
    );

    test!(
        case: windows_cr,
        text: "/**\r * foo\r */\r012345678\rabcdef012345678\ra",
        source_file_start_pos: 0,
        lines: vec![0, 4, 11, 15, 25, 41],
        multi_byte_chars: Vec::new(),
        non_narrow_chars: Vec::new(),
    );

    test!(
        case: windows_crlf,
        text: "/**\r\n * foo\r\n */\r\n012345678\r\nabcdef012345678\r\na",
        source_file_start_pos: 0,
        lines: vec![0, 5, 13, 18, 29, 46],
        multi_byte_chars: Vec::new(),
        non_narrow_chars: Vec::new(),
    );
}
