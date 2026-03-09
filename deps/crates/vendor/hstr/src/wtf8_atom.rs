use std::{
    fmt::Debug,
    hash::Hash,
    mem::{forget, transmute, ManuallyDrop},
    ops::Deref,
};

use debug_unreachable::debug_unreachable;

use crate::{
    macros::{get_hash, impl_from_alias, partial_eq},
    tagged_value::TaggedValue,
    wtf8::Wtf8,
    Atom, DYNAMIC_TAG, INLINE_TAG, LEN_MASK, LEN_OFFSET, TAG_MASK,
};

/// A WTF-8 encoded atom. This is like [Atom], but can contain unpaired
/// surrogates.
///
/// [Atom]: crate::Atom
#[repr(transparent)]
pub struct Wtf8Atom {
    pub(crate) unsafe_data: TaggedValue,
}

impl Wtf8Atom {
    #[inline(always)]
    pub fn new<S>(s: S) -> Self
    where
        Self: From<S>,
    {
        Self::from(s)
    }

    /// Try to convert this to a UTF-8 [Atom].
    ///
    /// Returns [Atom] if the string is valid UTF-8, otherwise returns
    /// the original [Wtf8Atom].
    pub fn try_into_atom(self) -> Result<Atom, Wtf8Atom> {
        if self.as_str().is_some() {
            let atom = ManuallyDrop::new(self);
            Ok(Atom {
                unsafe_data: atom.unsafe_data,
            })
        } else {
            Err(self)
        }
    }

    #[inline(always)]
    fn tag(&self) -> u8 {
        self.unsafe_data.tag() & TAG_MASK
    }

    /// Return true if this is a dynamic Atom.
    #[inline(always)]
    fn is_dynamic(&self) -> bool {
        self.tag() == DYNAMIC_TAG
    }
}

impl Default for Wtf8Atom {
    #[inline(never)]
    fn default() -> Self {
        Wtf8Atom::new("")
    }
}

/// Immutable, so it's safe to be shared between threads
unsafe impl Send for Wtf8Atom {}

/// Immutable, so it's safe to be shared between threads
unsafe impl Sync for Wtf8Atom {}

impl Debug for Wtf8Atom {
    #[inline]
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        Debug::fmt(&**self, f)
    }
}

#[cfg(feature = "serde")]
impl serde::ser::Serialize for Wtf8Atom {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::ser::Serializer,
    {
        use crate::wtf8::Wtf8;
        fn convert_wtf8_to_raw(s: &Wtf8) -> String {
            let mut result = String::new();
            let mut iter = s.code_points().peekable();

            while let Some(code_point) = iter.next() {
                if let Some(c) = code_point.to_char() {
                    // Escape literal '\u' sequences to avoid ambiguity with surrogate encoding.
                    // Without this escaping, we couldn't distinguish between:
                    // - JavaScript's "\uD800" (actual unpaired surrogate)
                    // - JavaScript's "\\uD800" (literal text '\uD800')
                    //
                    // By escaping literal '\u' to '\\u', we ensure:
                    // - Unpaired surrogates serialize as '\uXXXX'
                    // - Literal '\u' text serializes as '\\uXXXX'
                    //
                    // However, we should only escape '\u' if it's followed by exactly 4 hex digits,
                    // which would indicate a Unicode escape sequence. Otherwise, '\u' followed by
                    // non-hex characters (like '\util') should not be escaped.
                    if c == '\\' && iter.peek().map(|cp| cp.to_u32()) == Some('u' as u32) {
                        // Look ahead to see if this is followed by exactly 4 hex digits
                        let mut lookahead = iter.clone();
                        lookahead.next(); // skip 'u'

                        let mut hex_count = 0;
                        let mut all_hex = true;
                        for _ in 0..4 {
                            if let Some(next_cp) = lookahead.next() {
                                if let Some(next_c) = next_cp.to_char() {
                                    if next_c.is_ascii_hexdigit() {
                                        hex_count += 1;
                                    } else {
                                        all_hex = false;
                                        break;
                                    }
                                } else {
                                    all_hex = false;
                                    break;
                                }
                            } else {
                                all_hex = false;
                                break;
                            }
                        }

                        // Only escape if we have exactly 4 hex digits after '\u'
                        if hex_count == 4 && all_hex {
                            iter.next(); // skip 'u'
                            result.push_str("\\\\u");
                        } else {
                            result.push(c);
                        }
                    } else {
                        result.push(c)
                    }
                } else {
                    // Unpaired surrogates can't be represented in valid UTF-8,
                    // so encode them as '\uXXXX' for JavaScript compatibility
                    result.push_str(format!("\\u{:04X}", code_point.to_u32()).as_str());
                }
            }

            result
        }

        serializer.serialize_str(&convert_wtf8_to_raw(self))
    }
}

#[cfg(feature = "serde")]
impl<'de> serde::de::Deserialize<'de> for Wtf8Atom {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        use crate::wtf8::{CodePoint, Wtf8Buf};
        fn convert_wtf8_string_to_wtf8(s: String) -> Wtf8Buf {
            let mut iter = s.chars().peekable();
            let mut result = Wtf8Buf::with_capacity(s.len());

            // This function reverses the encoding done in serialize.
            // It handles two cases:
            // 1. '\uXXXX' - Decode as an unpaired surrogate code point
            // 2. '\\uXXXX' - Treat as literal text '\uXXXX'
            while let Some(c) = iter.next() {
                if c == '\\' {
                    if iter.peek() == Some(&'u') {
                        // Found '\u' - might be a surrogate encoding
                        let _ = iter.next(); // skip 'u'

                        // Try to read 4 hex digits
                        let d1 = iter.next();
                        let d2 = iter.next();
                        let d3 = iter.next();
                        let d4 = iter.next();

                        if d1.is_some() && d2.is_some() && d3.is_some() && d4.is_some() {
                            let hex = format!(
                                "{}{}{}{}",
                                d1.unwrap(),
                                d2.unwrap(),
                                d3.unwrap(),
                                d4.unwrap()
                            );
                            if let Ok(code_point) = u16::from_str_radix(&hex, 16) {
                                result.push(unsafe {
                                    CodePoint::from_u32_unchecked(code_point as u32)
                                });
                                continue;
                            }
                        }

                        result.push_char('\\');
                        result.push_char('u');

                        macro_rules! push_if_some {
                            ($expr:expr) => {
                                if let Some(c) = $expr {
                                    result.push_char(c);
                                }
                            };
                        }

                        push_if_some!(d1);
                        push_if_some!(d2);
                        push_if_some!(d3);
                        push_if_some!(d4);
                    } else if iter.peek() == Some(&'\\') {
                        // Found '\\' - this is an escaped backslash
                        // '\\u' should become literal '\u' text
                        let _ = iter.next(); // skip the second '\'
                        if iter.peek() == Some(&'u') {
                            let _ = iter.next(); // skip 'u'
                            result.push_char('\\');
                            result.push_char('u');
                        } else {
                            result.push_str("\\\\");
                        }
                    } else {
                        result.push_char(c);
                    }
                } else {
                    result.push_char(c);
                }
            }
            result
        }

        String::deserialize(deserializer).map(|v| convert_wtf8_string_to_wtf8(v).into())
    }
}

impl PartialEq for Wtf8Atom {
    #[inline(never)]
    fn eq(&self, other: &Self) -> bool {
        partial_eq!(self, other);

        // If the store is different, the string may be the same, even though the
        // `unsafe_data` is different
        self.as_wtf8() == other.as_wtf8()
    }
}

impl Eq for Wtf8Atom {}

impl Hash for Wtf8Atom {
    #[inline(always)]
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        state.write_u64(self.get_hash());
    }
}

impl Drop for Wtf8Atom {
    #[inline(always)]
    fn drop(&mut self) {
        if self.is_dynamic() {
            unsafe { drop(crate::dynamic::restore_arc(self.unsafe_data)) }
        }
    }
}

impl Clone for Wtf8Atom {
    #[inline(always)]
    fn clone(&self) -> Self {
        Self::from_alias(self.unsafe_data)
    }
}

impl Deref for Wtf8Atom {
    type Target = Wtf8;

    #[inline(always)]
    fn deref(&self) -> &Self::Target {
        self.as_wtf8()
    }
}

impl AsRef<Wtf8> for Wtf8Atom {
    #[inline(always)]
    fn as_ref(&self) -> &Wtf8 {
        self.as_wtf8()
    }
}

impl PartialEq<Wtf8> for Wtf8Atom {
    #[inline]
    fn eq(&self, other: &Wtf8) -> bool {
        self.as_wtf8() == other
    }
}

impl PartialEq<crate::Atom> for Wtf8Atom {
    #[inline]
    fn eq(&self, other: &crate::Atom) -> bool {
        self.as_str() == Some(other.as_str())
    }
}

impl PartialEq<&'_ Wtf8> for Wtf8Atom {
    #[inline]
    fn eq(&self, other: &&Wtf8) -> bool {
        self.as_wtf8() == *other
    }
}

impl PartialEq<Wtf8Atom> for Wtf8 {
    #[inline]
    fn eq(&self, other: &Wtf8Atom) -> bool {
        self == other.as_wtf8()
    }
}

impl PartialEq<str> for Wtf8Atom {
    #[inline]
    fn eq(&self, other: &str) -> bool {
        matches!(self.as_str(), Some(s) if s == other)
    }
}

impl PartialEq<&str> for Wtf8Atom {
    #[inline]
    fn eq(&self, other: &&str) -> bool {
        matches!(self.as_str(), Some(s) if s == *other)
    }
}

impl Wtf8Atom {
    pub(super) fn get_hash(&self) -> u64 {
        get_hash!(self)
    }

    fn as_wtf8(&self) -> &Wtf8 {
        match self.tag() {
            DYNAMIC_TAG => unsafe {
                let item = crate::dynamic::deref_from(self.unsafe_data);
                Wtf8::from_bytes_unchecked(transmute::<&[u8], &'static [u8]>(&item.slice))
            },
            INLINE_TAG => {
                let len = (self.unsafe_data.tag() & LEN_MASK) >> LEN_OFFSET;
                let src = self.unsafe_data.data();
                unsafe { Wtf8::from_bytes_unchecked(&src[..(len as usize)]) }
            }
            _ => unsafe { debug_unreachable!() },
        }
    }
}

impl_from_alias!(Wtf8Atom);

#[cfg(test)]
impl Wtf8Atom {
    pub(crate) fn ref_count(&self) -> usize {
        match self.tag() {
            DYNAMIC_TAG => {
                let ptr = unsafe { crate::dynamic::deref_from(self.unsafe_data) };

                triomphe::ThinArc::strong_count(&ptr.0)
            }
            _ => 1,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::wtf8::{CodePoint, Wtf8Buf};

    #[test]
    fn test_serialize_normal_utf8() {
        let atom = Wtf8Atom::new("Hello, world!");
        let serialized = serde_json::to_string(&atom).unwrap();
        assert_eq!(serialized, "\"Hello, world!\"");
    }

    #[test]
    fn test_deserialize_normal_utf8() {
        let json = "\"Hello, world!\"";
        let atom: Wtf8Atom = serde_json::from_str(json).unwrap();
        assert_eq!(atom.as_str(), Some("Hello, world!"));
    }

    #[test]
    fn test_serialize_unpaired_high_surrogate() {
        // Create a WTF-8 string with an unpaired high surrogate (U+D800)
        let mut wtf8 = Wtf8Buf::new();
        wtf8.push(unsafe { CodePoint::from_u32_unchecked(0xd800) });
        let atom = Wtf8Atom::from(wtf8);

        let serialized = serde_json::to_string(&atom).unwrap();
        // The serialized output will have double escaping due to serde_json
        assert_eq!(serialized, "\"\\\\uD800\"");
    }

    #[test]
    fn test_serialize_unpaired_low_surrogate() {
        // Create a WTF-8 string with an unpaired low surrogate (U+DC00)
        let mut wtf8 = Wtf8Buf::new();
        wtf8.push(unsafe { CodePoint::from_u32_unchecked(0xdc00) });
        let atom = Wtf8Atom::from(wtf8);

        let serialized = serde_json::to_string(&atom).unwrap();
        // The serialized output will have double escaping due to serde_json
        assert_eq!(serialized, "\"\\\\uDC00\"");
    }

    #[test]
    fn test_serialize_multiple_surrogates() {
        // Create a WTF-8 string with multiple unpaired surrogates
        let mut wtf8 = Wtf8Buf::new();
        wtf8.push_str("Hello ");
        wtf8.push(unsafe { CodePoint::from_u32_unchecked(0xd800) });
        wtf8.push_str(" World ");
        wtf8.push(unsafe { CodePoint::from_u32_unchecked(0xdc00) });
        let atom = Wtf8Atom::from(wtf8);

        let serialized = serde_json::to_string(&atom).unwrap();
        // The serialized output will have double escaping due to serde_json
        assert_eq!(serialized, "\"Hello \\\\uD800 World \\\\uDC00\"");
    }

    #[test]
    fn test_serialize_literal_backslash_u() {
        // Test that literal "\u" in the string gets escaped properly
        let atom = Wtf8Atom::new("\\u0041");
        let serialized = serde_json::to_string(&atom).unwrap();
        // serde_json escapes the backslash, resulting in 4 backslashes
        assert_eq!(serialized, "\"\\\\\\\\u0041\"");
    }

    #[test]
    fn test_deserialize_escaped_backslash_u() {
        // Test deserializing the escaped format for unpaired surrogates
        let json = "\"\\\\uD800\"";
        let atom: Wtf8Atom = serde_json::from_str(json).unwrap();
        // This should be parsed as an unpaired surrogate
        assert_eq!(atom.as_str(), None);
        assert_eq!(atom.to_string_lossy(), "\u{FFFD}");
    }

    #[test]
    fn test_deserialize_unpaired_surrogates() {
        let json = "\"\\\\uD800\""; // Use escaped format that matches serialization
        let atom: Wtf8Atom = serde_json::from_str(json).unwrap();
        // Should contain an unpaired surrogate, so as_str() returns None
        assert_eq!(atom.as_str(), None);
        // But to_string_lossy should work
        assert_eq!(atom.to_string_lossy(), "\u{FFFD}");
    }

    #[test]
    fn test_round_trip_normal_string() {
        let original = Wtf8Atom::new("Hello, 世界! 🌍");
        let serialized = serde_json::to_string(&original).unwrap();
        let deserialized: Wtf8Atom = serde_json::from_str(&serialized).unwrap();
        assert_eq!(original.as_str(), deserialized.as_str());
    }

    #[test]
    fn test_round_trip_unpaired_surrogates() {
        // Create a string with unpaired surrogates
        let mut wtf8 = Wtf8Buf::new();
        wtf8.push_str("Before ");
        wtf8.push(unsafe { CodePoint::from_u32_unchecked(0xd800) });
        wtf8.push_str(" Middle ");
        wtf8.push(unsafe { CodePoint::from_u32_unchecked(0xdc00) });
        wtf8.push_str(" After");
        let original = Wtf8Atom::from(wtf8);

        let serialized = serde_json::to_string(&original).unwrap();
        let deserialized: Wtf8Atom = serde_json::from_str(&serialized).unwrap();

        // Both should be equal when compared as WTF-8
        assert_eq!(original, deserialized);

        // Both should produce the same lossy string
        assert_eq!(original.to_string_lossy(), deserialized.to_string_lossy());
    }

    #[test]
    fn test_round_trip_mixed_content() {
        // Create a complex string with normal text, emojis, and unpaired surrogates
        let mut wtf8 = Wtf8Buf::new();
        wtf8.push_str("Hello 世界 🌍 ");
        wtf8.push(unsafe { CodePoint::from_u32_unchecked(0xd83d) }); // Unpaired high
        wtf8.push_str(" test ");
        wtf8.push(unsafe { CodePoint::from_u32_unchecked(0xdca9) }); // Unpaired low
        let original = Wtf8Atom::from(wtf8);

        let serialized = serde_json::to_string(&original).unwrap();
        let deserialized: Wtf8Atom = serde_json::from_str(&serialized).unwrap();

        assert_eq!(original, deserialized);
    }

    #[test]
    fn test_empty_string() {
        let atom = Wtf8Atom::new("");
        let serialized = serde_json::to_string(&atom).unwrap();
        assert_eq!(serialized, "\"\"");

        let deserialized: Wtf8Atom = serde_json::from_str("\"\"").unwrap();
        assert_eq!(deserialized.as_str(), Some(""));
    }

    #[test]
    fn test_special_characters() {
        let test_cases = vec![
            ("\"", "\"\\\"\""),
            ("\n\r\t", "\"\\n\\r\\t\""), // serde_json escapes control characters
            ("\\", "\"\\\\\""),
            ("/", "\"/\""),
        ];

        for (input, expected) in test_cases {
            let atom = Wtf8Atom::new(input);
            let serialized = serde_json::to_string(&atom).unwrap();
            assert_eq!(serialized, expected, "Failed for input: {input:?}");

            let deserialized: Wtf8Atom = serde_json::from_str(&serialized).unwrap();
            assert_eq!(deserialized.as_str(), Some(input));
        }
    }

    #[test]
    fn test_consecutive_surrogates_not_paired() {
        // Test that consecutive surrogates that don't form a valid pair
        // are handled correctly
        let mut wtf8 = Wtf8Buf::new();
        wtf8.push(unsafe { CodePoint::from_u32_unchecked(0xd800) }); // High surrogate
        wtf8.push(unsafe { CodePoint::from_u32_unchecked(0xd800) }); // Another high surrogate
        let atom = Wtf8Atom::from(wtf8);

        let serialized = serde_json::to_string(&atom).unwrap();
        // The serialized output will have double escaping due to serde_json
        assert_eq!(serialized, "\"\\\\uD800\\\\uD800\"");

        let deserialized: Wtf8Atom = serde_json::from_str(&serialized).unwrap();
        assert_eq!(atom, deserialized);
    }

    #[test]
    fn test_deserialize_incomplete_escape() {
        // Test handling of incomplete escape sequences from our custom format
        let json = "\"\\\\\\\\u123\""; // Escaped backslash + incomplete sequence
        let atom: Wtf8Atom = serde_json::from_str(json).unwrap();
        // JSON decodes \\\\u123 to \\u123, then our deserializer sees \u123 and treats
        // it as literal
        assert_eq!(atom.as_str(), Some("\\u123"));
    }

    #[test]
    fn test_deserialize_invalid_hex() {
        // Test handling of invalid hex in escape sequences from our custom format
        let json = "\"\\\\\\\\uGGGG\""; // Escaped backslash + invalid hex
        let atom: Wtf8Atom = serde_json::from_str(json).unwrap();
        // JSON decodes \\\\uGGGG to \\uGGGG, then our deserializer sees \uGGGG and
        // treats it as literal
        assert_eq!(atom.as_str(), Some("\\uGGGG"));
    }

    #[test]
    fn test_try_into_atom_valid_utf8() {
        let wtf8_atom = Wtf8Atom::new("Valid UTF-8 string");
        let result = wtf8_atom.try_into_atom();
        assert!(result.is_ok());
        assert_eq!(result.unwrap().as_str(), "Valid UTF-8 string");
    }

    #[test]
    fn test_try_into_atom_invalid_utf8() {
        // Create an atom with unpaired surrogates
        let mut wtf8 = Wtf8Buf::new();
        wtf8.push(unsafe { CodePoint::from_u32_unchecked(0xd800) });
        let wtf8_atom = Wtf8Atom::from(wtf8);

        let result = wtf8_atom.try_into_atom();
        assert!(result.is_err());
        // Should return the original Wtf8Atom
        let err_atom = result.unwrap_err();
        assert_eq!(err_atom.to_string_lossy(), "\u{FFFD}");
    }

    #[test]
    fn test_backslash_util_issue_11214() {
        let atom =
            Wtf8Atom::from("C:\\github\\swc-plugin-coverage-instrument\\spec\\util\\verifier.ts");
        let serialized = serde_json::to_string(&atom).unwrap();

        assert!(
            !serialized.contains("spec\\\\\\\\util"),
            "Found quadruple backslashes in spec segment! Serialized: {serialized}"
        );

        assert!(
            serialized.contains("spec\\\\util"),
            "Expected double backslashes in spec segment not found! Serialized: {serialized}",
        );

        // The expected serialized value should have consistent escaping
        let expected = r#""C:\\github\\swc-plugin-coverage-instrument\\spec\\util\\verifier.ts""#;
        assert_eq!(
            serialized, expected,
            "Serialized value should have consistent backslash escaping"
        );

        // Test round-trip
        let deserialized: Wtf8Atom = serde_json::from_str(&serialized).unwrap();
        assert_eq!(atom, deserialized);
    }
}
