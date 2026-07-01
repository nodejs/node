// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use icu_provider::prelude::*;
use regex_automata::dfa::sparse::DFA;
use zerovec::VarZeroCow;

/// A serde-compatible version of [`regex_automata::dfa::sparse::DFA`].
///
/// This does not implement
/// [`serde::Deserialize`] directly, as binary deserialization is not supported in big-endian
/// platforms. `Self::maybe_deserialize` can be used to deserialize to `Option<SerdeDFA>`.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Clone, Debug, yoke::Yokeable, zerofrom::ZeroFrom)]
pub struct SerdeDFA<'data> {
    // Safety: These always represent a valid DFA (DFA::from_bytes(dfa_bytes).is_ok())
    dfa_bytes: VarZeroCow<'data, [u8]>,
    #[cfg(feature = "serde_human")]
    pattern: Option<alloc::borrow::Cow<'data, str>>,
}

impl PartialEq for SerdeDFA<'_> {
    fn eq(&self, other: &Self) -> bool {
        self.dfa_bytes == other.dfa_bytes
    }
}

#[cfg(feature = "datagen")]
impl databake::Bake for SerdeDFA<'_> {
    fn bake(&self, env: &databake::CrateEnv) -> databake::TokenStream {
        env.insert("icu_list");
        let le_bytes = databake::Bake::bake(&self.deref().to_bytes_little_endian().as_slice(), env);
        let be_bytes = databake::Bake::bake(&self.deref().to_bytes_big_endian().as_slice(), env);
        // Safe because of `to_bytes_little_endian`/`to_bytes_big_endian`'s invariant: They produce
        // valid DFA representations, and we consume them correctly taking care of the endianness of the target platform.
        databake::quote! {
            unsafe {
                icu_list::provider::SerdeDFA::from_dfa_bytes_unchecked(
                    if cfg!(target_endian = "little") {
                        #le_bytes
                    } else {
                        #be_bytes
                    }
                )
            }
        }
    }
}

#[cfg(feature = "datagen")]
impl databake::BakeSize for SerdeDFA<'_> {
    fn borrows_size(&self) -> usize {
        self.deref().write_to_len()
    }
}

#[cfg(feature = "datagen")]
impl serde::Serialize for SerdeDFA<'_> {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::ser::Serializer,
    {
        #[cfg(feature = "serde_human")]
        if serializer.is_human_readable() {
            return self
                .pattern
                .as_ref()
                .map(|pattern| pattern.serialize(serializer))
                .unwrap_or_else(|| {
                    use serde::ser::Error;
                    Err(S::Error::custom(
                        "cannot serialize a binary-deserialized SerdeDFA to JSON",
                    ))
                });
        }
        serializer.serialize_bytes(&self.deref().to_bytes_little_endian())
    }
}

#[cfg(feature = "serde")]
impl<'data> SerdeDFA<'data> {
    /// Deserializes to `Option<Self>`. Will return `None` for non-human-readable serialization
    /// formats on big-endian systems, as `regex_automata` serialization is endian-sensitive.
    pub fn maybe_deserialize<'de: 'data, D>(deserializer: D) -> Result<Option<Self>, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        use serde::Deserialize;

        #[cfg(feature = "serde_human")]
        if deserializer.is_human_readable() {
            use alloc::string::ToString;
            use serde::de::Error;
            return SerdeDFA::new(alloc::borrow::Cow::<str>::deserialize(deserializer)?)
                .map(Some)
                .map_err(|e| D::Error::custom(e.to_string()));
        }

        let dfa_bytes = VarZeroCow::<[u8]>::deserialize(deserializer)?;

        if cfg!(target_endian = "big") {
            return Ok(None);
        }

        // Verify safety invariant
        DFA::from_bytes(&dfa_bytes).map_err(|_e| {
            use serde::de::Error;
            D::Error::custom("Invalid DFA bytes")
        })?;

        Ok(Some(SerdeDFA {
            dfa_bytes,
            #[cfg(feature = "serde_human")]
            pattern: None,
        }))
    }
}

impl<'data> SerdeDFA<'data> {
    /// Creates a `SerdeDFA` from raw bytes. Used internally by databake.
    ///
    /// # Safety
    ///
    /// `dfa_bytes` has to be a valid DFA (`regex_automata::dfa::sparse::DFA::from_bytes(dfa_bytes).is_ok()`)
    pub const unsafe fn from_dfa_bytes_unchecked(dfa_bytes: &'data [u8]) -> Self {
        Self {
            // SAFETY: safe for VarZeroCow<[u8]>
            dfa_bytes: unsafe { VarZeroCow::from_bytes_unchecked(dfa_bytes) },
            #[cfg(feature = "serde_human")]
            pattern: None,
        }
    }

    /// Creates a `SerdeDFA` from a regex.
    #[cfg(any(feature = "datagen", feature = "serde_human",))]
    pub fn new(pattern: alloc::borrow::Cow<'data, str>) -> Result<Self, DataError> {
        use regex_automata::dfa::dense::{Builder, Config};

        let Some(anchored_pattern) = pattern.strip_prefix('^') else {
            return Err(
                DataError::custom("Only anchored regexes (starting with ^) are supported")
                    .with_display_context(&pattern),
            );
        };

        let mut builder = Builder::new();
        let dfa = builder
            .configure(
                Config::new()
                    .start_kind(regex_automata::dfa::StartKind::Anchored)
                    .minimize(true),
            )
            .build(anchored_pattern)
            .map_err(|e| {
                DataError::custom("Cannot build DFA")
                    .with_display_context(anchored_pattern)
                    .with_debug_context(&e)
            })?
            .to_sparse()
            .map_err(|e| {
                DataError::custom("Cannot sparsify DFA")
                    .with_display_context(anchored_pattern)
                    .with_debug_context(&e)
            })?;

        Ok(Self {
            dfa_bytes: VarZeroCow::new_owned(dfa.to_bytes_native_endian().into_boxed_slice()),
            pattern: Some(pattern),
        })
    }

    /// Returns the represented [`DFA`]
    #[expect(clippy::unwrap_used)] // by invariant
    pub fn deref(&'data self) -> DFA<&'data [u8]> {
        // Safe due to struct invariant.
        unsafe { DFA::from_bytes_unchecked(&self.dfa_bytes).unwrap().0 }
    }
}

#[cfg(all(test, feature = "datagen"))]
mod test {
    use super::*;
    use regex_automata::Input;
    use std::borrow::Cow;

    #[test]
    fn test_serde_dfa() {
        use regex_automata::dfa::Automaton;

        let matcher = SerdeDFA::new(Cow::Borrowed("^abc")).unwrap();

        assert!(matcher
            .deref()
            .try_search_fwd(&Input::new("ab").anchored(regex_automata::Anchored::Yes))
            .unwrap()
            .is_none());
        assert!(matcher
            .deref()
            .try_search_fwd(&Input::new("abc").anchored(regex_automata::Anchored::Yes))
            .unwrap()
            .is_some());
        assert!(matcher
            .deref()
            .try_search_fwd(&Input::new("abcde").anchored(regex_automata::Anchored::Yes))
            .unwrap()
            .is_some());
        assert!(matcher
            .deref()
            .try_search_fwd(&Input::new(" abcde").anchored(regex_automata::Anchored::Yes))
            .unwrap()
            .is_none());
    }

    #[derive(serde::Deserialize)]
    struct OptionSerdeDFA<'data>(
        #[serde(borrow, deserialize_with = "SerdeDFA::maybe_deserialize")] Option<SerdeDFA<'data>>,
    );

    #[test]
    #[cfg(target_endian = "little")]
    fn test_postcard_serialization() {
        let matcher = SerdeDFA::new(Cow::Borrowed("^abc*")).unwrap();

        let mut bytes = postcard::to_stdvec(&matcher).unwrap();
        assert_eq!(
            postcard::from_bytes::<OptionSerdeDFA>(&bytes).unwrap().0,
            Some(matcher)
        );

        // A corrupted byte leads to an error
        bytes[17] ^= 255;
        assert!(postcard::from_bytes::<OptionSerdeDFA>(&bytes).is_err());
        bytes[17] ^= 255;

        // An extra byte leads to an error
        bytes.insert(123, 40);
        assert!(postcard::from_bytes::<OptionSerdeDFA>(&bytes).is_err());
        bytes.remove(123);

        // Missing bytes lead to an error
        assert!(postcard::from_bytes::<OptionSerdeDFA>(&bytes[0..bytes.len() - 5]).is_err());
    }

    #[test]
    fn test_rmp_serialization() {
        let matcher = SerdeDFA::new(Cow::Borrowed("^abc*")).unwrap();

        let bytes = rmp_serde::to_vec(&matcher).unwrap();
        assert_eq!(
            rmp_serde::from_slice::<OptionSerdeDFA>(&bytes).unwrap().0,
            Some(matcher)
        );
    }

    #[test]
    #[cfg(feature = "serde_human")]
    fn test_json_serialization() {
        let matcher = SerdeDFA::new(Cow::Borrowed("^abc*")).unwrap();

        let json = serde_json::to_string(&matcher).unwrap();
        assert_eq!(
            serde_json::from_str::<OptionSerdeDFA>(&json).unwrap().0,
            Some(matcher)
        );
        assert!(serde_json::from_str::<OptionSerdeDFA>(".*[").is_err());
    }

    #[test]
    fn databake() {
        // This is the DFA for ".*"
        databake::test_bake!(
            SerdeDFA,
            const,
            unsafe {
                crate::provider::SerdeDFA::from_dfa_bytes_unchecked(
                    if cfg!(target_endian = "little") {
                        b"rust-regex-automata-dfa-sparse\0\0\xFF\xFE\0\0\x02\0\0\0\0\0\0\0\x02\0\0\0\x0E\0\0\0\x01\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x01\x02\x02\x02\x03\x04\x04\x05\x06\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x08\t\t\t\n\x0B\x0B\x0C\r\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x12\x12\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x14\x15\x15\x15\x15\x15\x15\x15\x15\x15\x15\x15\x15\x16\x17\x17\x18\x19\x19\x19\x1A\x1B\x1B\x1B\x1B\x1B\x1B\x1B\x1B\x1B\x1B\x1B(\x01\0\0\x01\0\0\0\0\0\0\0\0\x01\0\0\0\0\0\0\0\0\x01\x80\0\0\0\0\0\0\x01\0\0\0\0\0\0\0\0\x05\0\x05\x05\x06\x06\x0C\x0C\r\r\0\0S\0\0\0D\0\0\0S\0\0\0D\0\0\0\0\0\0\0\0\x02\0\0\x1B\0\0\x12\0\0\0\x12\0\0\0\0\x03\0\x06\x06\r\r\0\0h\0\0\0h\0\0\0\0\0\0\0\0\x0E\0\0\0\x02\x02\x04\x07\t\t\x0B\x0E\x13\x13\x14\x14\x15\x15\x16\x16\x17\x17\x18\x18\x19\x19\x1A\x1A\0\0D\0\0\0D\0\0\0D\0\0\0D\0\0\0D\0\0\0\xBF\0\0\0\xCE\0\0\0\xDD\0\0\0\xEC\0\0\0\xDD\0\0\0\xFB\0\0\0\n\x01\0\0\x19\x01\0\0\x12\0\0\0\0\x02\0\x0F\x11\0\0D\0\0\0\0\0\0\0\0\x02\0\x11\x11\0\0\xBF\0\0\0\0\0\0\0\0\x02\0\x0F\x11\0\0\xBF\0\0\0\0\0\0\0\0\x02\0\x0F\x10\0\0\xBF\0\0\0\0\0\0\0\0\x02\0\x10\x11\0\0\xDD\0\0\0\0\0\0\0\0\x02\0\x0F\x11\0\0\xDD\0\0\0\0\0\0\0\0\x02\0\x0F\x0F\0\0\xDD\0\0\0\0\0\0\0\0\x02\0\0\0\0\0\0\0\0\0\0\0\0\0\x03\0\0\x04\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\0\0\0\0\0\0\0\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\0\0\0\0\x01\0\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x06\0\0\0\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF`\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0#\0\0\0#\0\0\0#\0\0\0#\0\0\0#\0\0\0#\0\0\0\x12\0\0\0\t\0\0\0\x12\0\0\0\x12\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                    } else {
                        b"rust-regex-automata-dfa-sparse\0\0\0\0\xFE\xFF\0\0\0\x02\0\0\0\0\0\0\0\x02\0\0\0\x0E\0\0\0\x01\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x01\x02\x02\x02\x03\x04\x04\x05\x06\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x08\t\t\t\n\x0B\x0B\x0C\r\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0E\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x0F\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x12\x12\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x13\x14\x15\x15\x15\x15\x15\x15\x15\x15\x15\x15\x15\x15\x16\x17\x17\x18\x19\x19\x19\x1A\x1B\x1B\x1B\x1B\x1B\x1B\x1B\x1B\x1B\x1B\x1B\0\0\x01(\0\x01\0\0\0\0\0\0\0\0\x01\0\0\0\0\0\0\0\x80\x01\0\0\0\0\0\0\0\0\0\x01\0\0\0\0\0\0\x05\x05\x05\x06\x06\x0C\x0C\r\r\0\0\0\0\0S\0\0\0D\0\0\0S\0\0\0D\0\0\0\0\0\0\x02\0\x1B\0\0\0\0\0\x12\0\0\0\x12\0\0\x03\x06\x06\r\r\0\0\0\0\0h\0\0\0h\0\0\0\0\0\0\x0E\0\0\x02\x02\x04\x07\t\t\x0B\x0E\x13\x13\x14\x14\x15\x15\x16\x16\x17\x17\x18\x18\x19\x19\x1A\x1A\0\0\0\0\0D\0\0\0D\0\0\0D\0\0\0D\0\0\0D\0\0\0\xBF\0\0\0\xCE\0\0\0\xDD\0\0\0\xEC\0\0\0\xDD\0\0\0\xFB\0\0\x01\n\0\0\x01\x19\0\0\0\x12\0\0\x02\x0F\x11\0\0\0\0\0D\0\0\0\0\0\0\x02\x11\x11\0\0\0\0\0\xBF\0\0\0\0\0\0\x02\x0F\x11\0\0\0\0\0\xBF\0\0\0\0\0\0\x02\x0F\x10\0\0\0\0\0\xBF\0\0\0\0\0\0\x02\x10\x11\0\0\0\0\0\xDD\0\0\0\0\0\0\x02\x0F\x11\0\0\0\0\0\xDD\0\0\0\0\0\0\x02\x0F\x0F\0\0\0\0\0\xDD\0\0\0\0\0\0\0\0\x02\0\0\0\0\0\0\0\0\0\0\x03\0\0\x04\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\0\0\0\0\0\0\0\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\0\0\0\0\x01\0\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x06\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\0\0\0`\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0#\0\0\0#\0\0\0#\0\0\0#\0\0\0#\0\0\0#\0\0\0\x12\0\0\0\t\0\0\0\x12\0\0\0\x12\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
                    },
                )
            },
            icu_list
        );
    }
}
