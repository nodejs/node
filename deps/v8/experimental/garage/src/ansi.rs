// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! SGR escape handling.
//!
//! d8 does not check `isatty`, so a piped trace carries colour escapes mid-line
//! (spike-findings.md §1). Every consumer of trace text therefore has to decide
//! what to do with them, and the answer differs by consumer:
//!
//! - the **parser** matches on stripped text,
//! - the **raw view** wants d8's own colours back (Phase 4.1),
//! - Phase 1's scaffolding viewport strips them, because emitting raw escapes
//!   through the TUI cell buffer would corrupt the screen rather than colour it.
//!
//! So stripping is a view-time transform over borrowed bytes, not something
//! done once on load: the original bytes stay untouched in the buffer.

/// Strips CSI sequences, returning borrowed bytes when there is nothing to do.
///
/// Only CSI (`ESC [` … final byte) is recognised, which is all d8 emits. Other
/// escapes are passed through — this is a viewer, and inventing an
/// interpretation for bytes we do not understand is how data gets lost.
pub fn strip(bytes: &[u8]) -> std::borrow::Cow<'_, [u8]> {
    if !bytes.contains(&0x1b) {
        return std::borrow::Cow::Borrowed(bytes);
    }

    let mut out = Vec::with_capacity(bytes.len());
    let mut i = 0;
    while i < bytes.len() {
        if bytes[i] == 0x1b && bytes.get(i + 1) == Some(&b'[') {
            i += 2;
            // Parameter and intermediate bytes, then one final byte in 0x40..=0x7e.
            while i < bytes.len() && !(0x40..=0x7e).contains(&bytes[i]) {
                i += 1;
            }
            i += 1; // the final byte itself
        } else {
            out.push(bytes[i]);
            i += 1;
        }
    }
    std::borrow::Cow::Owned(out)
}

/// Renders a trace line as something safe to hand to a terminal widget:
/// escapes stripped, tabs expanded, other control bytes replaced.
///
/// Lossy on purpose. Trace files are not guaranteed UTF-8 (a `--print-code`
/// dump can contain arbitrary bytes), and a viewer that refuses to display
/// invalid input is worse than one that shows a replacement character.
pub fn to_display_string(bytes: &[u8]) -> String {
    let stripped = strip(bytes);
    let text = String::from_utf8_lossy(&stripped);

    // Tab counts as a control character, so this one test covers both cases.
    if !text.contains(char::is_control) {
        return text.into_owned();
    }

    let mut out = String::with_capacity(text.len());
    for ch in text.chars() {
        match ch {
            // Not tab-stop accurate; the raw view (Phase 4) can do better.
            '\t' => out.push_str("    "),
            c if c.is_control() => out.push('\u{fffd}'),
            c => out.push(c),
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn passes_through_clean_text() {
        assert!(matches!(
            strip(b"  12: Int32Add [n8, n9]"),
            std::borrow::Cow::Borrowed(_)
        ));
    }

    #[test]
    fn strips_mid_line_sgr() {
        // Shape taken from a real fixture: colour opens mid-line and closes.
        let line = b"\x1b[0;34m  1: \x1b[0mInt32Add\x1b[0m";
        assert_eq!(&*strip(line), b"  1: Int32Add");
    }

    #[test]
    fn strips_multi_parameter_sgr() {
        assert_eq!(&*strip(b"\x1b[1;38;5;208mx\x1b[m"), b"x");
    }

    #[test]
    fn leaves_lone_escape_alone() {
        // Not a CSI: pass it through rather than guess.
        assert_eq!(&*strip(b"a\x1bb"), b"a\x1bb");
    }

    #[test]
    fn unterminated_csi_consumes_to_end() {
        assert_eq!(&*strip(b"a\x1b[0;1"), b"a");
    }

    #[test]
    fn display_string_is_lossy_and_control_free() {
        assert_eq!(to_display_string(b"a\tb"), "a    b");
        assert_eq!(to_display_string(b"a\x07b"), "a\u{fffd}b");
        assert_eq!(to_display_string(b"a\xffb"), "a\u{fffd}b");
    }
}
