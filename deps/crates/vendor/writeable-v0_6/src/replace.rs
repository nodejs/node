// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::{impl_display_with_writeable, LengthHint, Writeable};
use core::fmt;

/// A [`Writeable`] adapter that replaces occurrences of a needle with a replacement.
///
/// This adapter performs the replacement in a streaming fashion during `write_to`,
/// requiring zero allocations.
///
/// # Examples
///
/// ```
/// use writeable::adapters::Replace;
/// use writeable::assert_writeable_eq;
/// use writeable::concat_writeable;
///
/// let source = concat_writeable!("I 💖 🦀", " and 🦀 loves me!");
/// let replace = Replace {
///     source,
///     needle: "🦀",
///     replacement: "Rust",
/// };
///
/// assert_writeable_eq!(replace, "I 💖 Rust and Rust loves me!");
/// ```
#[derive(Debug)]
#[allow(clippy::exhaustive_structs)] // designed for nesting
pub struct Replace<A, B, C> {
    /// The source writeable.
    pub source: A,
    /// The needle to search for.
    pub needle: B,
    /// The replacement writeable.
    pub replacement: C,
}

// Computes the Knuth-Morris-Pratt (KMP) prefix function (failure function) value
// for the character prefix ending at byte index `matched_bytes` in `needle`.
//
// Returns the byte length of the longest proper prefix of `needle[0..matched_bytes]`
// that is also a suffix of `needle[0..matched_bytes]`.
//
// This is computed on the fly without allocation by iterating over char boundaries.
fn get_pi_bytes(needle: &str, matched_bytes: usize) -> usize {
    let s = match needle.get(0..matched_bytes) {
        Some(s) => s,
        None => return 0,
    };
    // char_indices() gives us the byte offsets of character starts.
    // These offsets correspond to the byte lengths of all possible prefixes.
    // We want to iterate them in reverse order, excluding the first one (0)
    // because we want proper prefixes.
    for k in s
        .char_indices()
        .map(|(idx, _)| idx)
        .rev()
        .filter(|&idx| idx > 0)
    {
        // Compare the prefix of length `k` with the suffix of length `k`.
        if let Some(suffix) = s.as_bytes().get(s.len() - k..) {
            if s.as_bytes().starts_with(suffix) {
                return k;
            }
        }
    }
    0
}

// A writer wrapper that performs streaming replacement.
// It intercepts characters written to it, matches them against `needle` using KMP
// (tracking progress by storing the remaining unmatched suffix of the needle),
// and writes `replacement` when a full match is found, or the original characters otherwise.
struct ReplaceWriter<'a, W: ?Sized, C> {
    // The underlying sink to write to.
    sink: &'a mut W,
    // The needle we are searching for.
    needle: &'a str,
    // The replacement to write when the needle is matched.
    replacement: &'a C,
    // The remaining unmatched suffix of the needle.
    // This is always a suffix of `needle` starting at a character boundary.
    remaining_needle: &'a str,
}

impl<'a, W, C> ReplaceWriter<'a, W, C>
where
    W: fmt::Write + ?Sized,
    C: Writeable,
{
    fn new(sink: &'a mut W, needle: &'a str, replacement: &'a C) -> Self {
        Self {
            sink,
            needle,
            replacement,
            remaining_needle: needle,
        }
    }

    // Helper to get the length of the prefix matched so far.
    fn matched_len(&self) -> usize {
        self.needle.len() - self.remaining_needle.len()
    }

    // Finalizes the writer, flushing any partially matched prefix to the sink.
    fn finalize(&mut self) -> fmt::Result {
        let matched = self.matched_len();
        if matched > 0 {
            let slice = self.needle.get(0..matched).ok_or(fmt::Error)?;
            self.sink.write_str(slice)?;
            self.remaining_needle = self.needle;
        }
        Ok(())
    }
}

impl<'a, W, C> fmt::Write for ReplaceWriter<'a, W, C>
where
    W: fmt::Write + ?Sized,
    C: Writeable,
{
    fn write_str(&mut self, s: &str) -> fmt::Result {
        for c in s.chars() {
            self.write_char(c)?;
        }
        Ok(())
    }

    fn write_char(&mut self, c: char) -> fmt::Result {
        // If the needle is empty, we just pass through the characters.
        if self.needle.is_empty() {
            return self.sink.write_char(c);
        }

        let mut matched = self.matched_len();
        // KMP State Transition:
        // While we have a mismatch and we are not at the start of the needle,
        // backtrack using the prefix function.
        while matched > 0 && !self.remaining_needle.starts_with(c) {
            let old_j = matched;
            matched = get_pi_bytes(self.needle, old_j);
            // Since we backtracked, the prefix of length `old_j - j` is no longer
            // part of the potential match. We write it to the sink as a single slice.
            let slice = self.needle.get(0..(old_j - matched)).ok_or(fmt::Error)?;
            self.sink.write_str(slice)?;
            // Update remaining_needle to reflect the new matched length.
            self.remaining_needle = self.needle.get(matched..).ok_or(fmt::Error)?;
        }

        // If the character matches the next character in the needle, advance the match state.
        if self.remaining_needle.starts_with(c) {
            // Advance remaining_needle by the matched character.
            self.remaining_needle = self
                .remaining_needle
                .get(c.len_utf8()..)
                .ok_or(fmt::Error)?;
            if self.remaining_needle.is_empty() {
                // Full match found! Write the replacement instead of the needle.
                self.replacement.write_to(self.sink)?;
                // Reset match state.
                self.remaining_needle = self.needle;
            }
        } else {
            // Mismatch at the very beginning of the needle. Write the character as is.
            self.sink.write_char(c)?;
        }
        Ok(())
    }
}

impl<A, C> Writeable for Replace<A, &str, C>
where
    A: Writeable,
    C: Writeable,
{
    // We do not implement writeable_borrow because it is meant to be a constant-time O(1)
    // operation, but determining if a replacement occurred would require O(N) scanning.
    fn write_to<W: fmt::Write + ?Sized>(&self, sink: &mut W) -> fmt::Result {
        let mut writer = ReplaceWriter::new(sink, self.needle, &self.replacement);
        self.source.write_to(&mut writer)?;
        writer.finalize()
    }

    fn writeable_length_hint(&self) -> LengthHint {
        let source_hint = self.source.writeable_length_hint();
        let needle_len = self.needle.len();
        let replacement_hint = self.replacement.writeable_length_hint();

        // If needle and replacement have same exact length, length is unchanged.
        if let Some(r_upper) = replacement_hint.1 {
            if replacement_hint.0 == r_upper && needle_len == r_upper {
                return source_hint;
            }
        }

        let mut lower = 0;
        let mut upper = None;

        // If replacement is always larger than or equal to needle:
        // New length is at least the source length.
        if replacement_hint.0 >= needle_len {
            lower = source_hint.0;
        }

        // If replacement is always smaller than or equal to needle:
        // New length is at most the source length.
        if let Some(r_upper) = replacement_hint.1 {
            if r_upper <= needle_len {
                upper = source_hint.1;
            }
        }

        LengthHint(lower, upper)
    }
}

impl_display_with_writeable!(Replace<A, &'a str, C>, #[cfg(feature = "alloc")], where 'a, A: Writeable, C: Writeable);

#[test]
fn test_replace() {
    use crate::assert_writeable_eq;
    use crate::concat::Concat;

    // Basic replacement
    let replace1 = Replace {
        source: Concat("Hello", " 10 22 1101 33"),
        needle: "10",
        replacement: Concat("4", "4"),
    };
    assert_writeable_eq!(replace1, "Hello 44 22 1441 33");

    // Empty needle (should just write source)
    let replace2 = Replace {
        source: "Hello World",
        needle: "",
        replacement: "X",
    };
    assert_writeable_eq!(replace2, "Hello World");

    // Empty replacement
    let replace3 = Replace {
        source: "Hello 10 World 10",
        needle: "10",
        replacement: "",
    };
    assert_writeable_eq!(replace3, "Hello  World ");

    // Needle not found
    let replace4 = Replace {
        source: "Hello World",
        needle: "10",
        replacement: "X",
    };
    assert_writeable_eq!(replace4, "Hello World");

    // Needle at the beginning
    let replace5 = Replace {
        source: "10 Hello World",
        needle: "10",
        replacement: "X",
    };
    assert_writeable_eq!(replace5, "X Hello World");

    // Needle at the end
    let replace6 = Replace {
        source: "Hello World 10",
        needle: "10",
        replacement: "X",
    };
    assert_writeable_eq!(replace6, "Hello World X");

    // Overlapping needles (should consume and not match again)
    let replace7 = Replace {
        source: "ababa",
        needle: "aba",
        replacement: "X",
    };
    assert_writeable_eq!(replace7, "Xba");

    // Self-overlap but no match
    let replace8 = Replace {
        source: "aab",
        needle: "aac",
        replacement: "X",
    };
    assert_writeable_eq!(replace8, "aab");

    // Multi-byte UTF-8
    let replace9 = Replace {
        source: "🚀 🛸 🚀🚀 🚁",
        needle: "🚀",
        replacement: "星",
    };
    assert_writeable_eq!(replace9, "星 🛸 星星 🚁");

    // Multi-byte UTF-8 with partial match
    let replace10 = Replace {
        source: "🚀🚁",
        needle: "🚀🛸",
        replacement: "星",
    };
    assert_writeable_eq!(replace10, "🚀🚁");

    // Multi-byte UTF-8 with backtracking (no match)
    let replace11 = Replace {
        source: "🚀🚀🚁",
        needle: "🚀🚀🛸",
        replacement: "星",
    };
    assert_writeable_eq!(replace11, "🚀🚀🚁");

    // Multi-byte UTF-8 with backtracking (match)
    let replace12 = Replace {
        source: "🚀🚀🚀🛸",
        needle: "🚀🚀🛸",
        replacement: "星",
    };
    assert_writeable_eq!(replace12, "🚀星");
}
