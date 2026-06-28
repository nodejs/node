// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::internals::{CaseMapLocale, FoldOptions, FullCaseWriteable, StringAndWriteable};
use crate::provider::data::MappingKind;
use crate::provider::CaseMap;
use crate::provider::CaseMapV1;
use crate::set::ClosureSink;
use crate::titlecase::{LeadingAdjustment, TitlecaseOptions, TrailingCase};
use alloc::borrow::Cow;
use icu_locale_core::LanguageIdentifier;
use icu_provider::prelude::*;
use writeable::Writeable;

/// A struct with the ability to convert characters and strings to uppercase or lowercase,
/// or fold them to a normalized form for case-insensitive comparison.
///
/// Most methods for this type live on [`CaseMapperBorrowed`], which you can obtain via
/// [`CaseMapper::new()`] or [`CaseMapper::as_borrowed()`].
///
/// # Examples
///
/// ```rust
/// use icu::casemap::CaseMapper;
/// use icu::locale::langid;
///
/// let cm = CaseMapper::new();
///
/// assert_eq!(
///     cm.uppercase_to_string("hello world", &langid!("und")),
///     "HELLO WORLD"
/// );
/// assert_eq!(
///     cm.lowercase_to_string("Γειά σου Κόσμε", &langid!("und")),
///     "γειά σου κόσμε"
/// );
/// ```
#[derive(Clone, Debug)]
pub struct CaseMapper {
    pub(crate) data: DataPayload<CaseMapV1>,
}

impl AsRef<CaseMapper> for CaseMapper {
    fn as_ref(&self) -> &CaseMapper {
        self
    }
}

/// A struct with the ability to convert characters and strings to uppercase or lowercase,
/// or fold them to a normalized form for case-insensitive comparison, borrowed version.
///
/// See methods or [`CaseMapper`] for examples.
#[derive(Clone, Debug, Copy)]
pub struct CaseMapperBorrowed<'a> {
    pub(crate) data: &'a CaseMap<'a>,
}

impl CaseMapperBorrowed<'static> {
    /// Cheaply converts a [`CaseMapperBorrowed<'static>`] into a [`CaseMapper`].
    ///
    /// Note: Due to branching and indirection, using [`CaseMapper`] might inhibit some
    /// compile-time optimizations that are possible with [`CaseMapperBorrowed`].
    pub const fn static_to_owned(self) -> CaseMapper {
        CaseMapper {
            data: DataPayload::from_static_ref(self.data),
        }
    }
    /// Creates a [`CaseMapperBorrowed`] using compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::casemap::CaseMapper;
    /// use icu::locale::langid;
    ///
    /// let cm = CaseMapper::new();
    ///
    /// assert_eq!(
    ///     cm.uppercase_to_string("hello world", &langid!("und")),
    ///     "HELLO WORLD"
    /// );
    /// ```
    #[cfg(feature = "compiled_data")]
    pub const fn new() -> Self {
        Self {
            data: crate::provider::Baked::SINGLETON_CASE_MAP_V1,
        }
    }
}

#[cfg(feature = "compiled_data")]
impl Default for CaseMapperBorrowed<'static> {
    fn default() -> Self {
        Self::new()
    }
}

impl<'a> CaseMapperBorrowed<'a> {
    /// Returns the full lowercase mapping of the given string as a [`Writeable`].
    /// This function is context and language sensitive. Callers should pass the text's language
    /// as a `LanguageIdentifier` (usually the `id` field of the `Locale`) if available, or
    /// `Default::default()` for the root locale.
    ///
    /// See [`Self::lowercase_to_string()`] for the equivalent convenience function that returns a string,
    /// as well as for an example.
    pub fn lowercase(self, src: &'a str, langid: &LanguageIdentifier) -> impl Writeable + 'a {
        self.data.full_helper_writeable::<false>(
            src,
            CaseMapLocale::from_langid(langid),
            MappingKind::Lower,
            TrailingCase::default(),
        )
    }

    /// Returns the full uppercase mapping of the given string as a [`Writeable`].
    /// This function is context and language sensitive. Callers should pass the text's language
    /// as a `LanguageIdentifier` (usually the `id` field of the `Locale`) if available, or
    /// `Default::default()` for the root locale.
    ///
    /// See [`Self::uppercase_to_string()`] for the equivalent convenience function that returns a string,
    /// as well as for an example.
    pub fn uppercase(self, src: &'a str, langid: &LanguageIdentifier) -> impl Writeable + 'a {
        self.data.full_helper_writeable::<false>(
            src,
            CaseMapLocale::from_langid(langid),
            MappingKind::Upper,
            TrailingCase::default(),
        )
    }

    /// Returns the full titlecase mapping of the given string as a [`Writeable`], treating
    /// the string as a single segment (and thus only titlecasing the beginning of it). Performs
    /// the specified leading adjustment behavior from the options without loading additional data.
    ///
    /// This should typically be used as a lower-level helper to construct the titlecasing operation desired
    /// by the application, for example one can titlecase on a per-word basis by mixing this with
    /// a `WordSegmenter`.
    ///
    /// This function is context and language sensitive. Callers should pass the text's language
    /// as a `LanguageIdentifier` (usually the `id` field of the `Locale`) if available, or
    /// `Default::default()` for the root locale.
    ///
    /// This function performs "adjust to cased" leading adjustment behavior when [`LeadingAdjustment::Auto`] or [`LeadingAdjustment::ToCased`]
    /// is set. Auto mode is not able to pick the "adjust to letter/number/symbol" behavior as this type does not load
    /// the data to do so, use [`TitlecaseMapper`] if such behavior is desired. See
    /// the docs of [`TitlecaseMapper`] for more information on what this means. There is no difference between
    /// the behavior of this function and the equivalent ones on [`TitlecaseMapper`] when the head adjustment mode
    /// is [`LeadingAdjustment::None`].
    ///
    /// See [`Self::titlecase_segment_with_only_case_data_to_string()`] for the equivalent convenience function that returns a string,
    /// as well as for an example.
    ///
    /// [`TitlecaseMapper`]: crate::TitlecaseMapper
    pub fn titlecase_segment_with_only_case_data(
        self,
        src: &'a str,
        langid: &LanguageIdentifier,
        options: TitlecaseOptions,
    ) -> impl Writeable + 'a {
        self.titlecase_segment_with_adjustment(src, langid, options, |data, ch| data.is_cased(ch))
    }

    /// Helper to support different leading adjustment behaviors,
    /// `char_is_lead` is a function that returns true for a character that is allowed to be the
    /// first relevant character in a titlecasing string, when `leading_adjustment != None`
    ///
    /// We return a concrete type instead of `impl Trait` so the return value can be mixed with that of other calls
    /// to this function with different closures
    pub(crate) fn titlecase_segment_with_adjustment(
        self,
        src: &'a str,
        langid: &LanguageIdentifier,
        options: TitlecaseOptions,
        char_is_lead: impl Fn(&CaseMap, char) -> bool,
    ) -> StringAndWriteable<'a, FullCaseWriteable<'a, 'a, true>> {
        let (head, rest) = match options.leading_adjustment.unwrap_or_default() {
            LeadingAdjustment::Auto | LeadingAdjustment::ToCased => {
                let first_cased = src
                    .char_indices()
                    .find(|(_i, ch)| char_is_lead(self.data, *ch));
                if let Some((first_cased, _ch)) = first_cased {
                    (
                        src.get(..first_cased).unwrap_or(""),
                        src.get(first_cased..).unwrap_or(""),
                    )
                } else {
                    (src, "")
                }
            }
            LeadingAdjustment::None => ("", src),
        };
        let writeable = self.data.full_helper_writeable::<true>(
            rest,
            CaseMapLocale::from_langid(langid),
            MappingKind::Title,
            options.trailing_case.unwrap_or_default(),
        );
        StringAndWriteable {
            string: head,
            writeable,
        }
    }
    /// Case-folds the characters in the given string as a [`Writeable`].
    /// This function is locale-independent and context-insensitive.
    ///
    /// Can be used to test if two strings are case-insensitively equivalent.
    ///
    /// See [`Self::fold_string()`] for the equivalent convenience function that returns a string,
    /// as well as for an example.
    pub fn fold(self, src: &'a str) -> impl Writeable + 'a {
        self.data.full_helper_writeable::<false>(
            src,
            CaseMapLocale::Root,
            MappingKind::Fold,
            TrailingCase::default(),
        )
    }

    /// Case-folds the characters in the given string as a [`Writeable`],
    /// using Turkic (T) mappings for dotted/dotless I.
    /// This function is locale-independent and context-insensitive.
    ///
    /// Can be used to test if two strings are case-insensitively equivalent.
    ///
    /// See [`Self::fold_turkic_string()`] for the equivalent convenience function that returns a string,
    /// as well as for an example.
    pub fn fold_turkic(self, src: &'a str) -> impl Writeable + 'a {
        self.data.full_helper_writeable::<false>(
            src,
            CaseMapLocale::Turkish,
            MappingKind::Fold,
            TrailingCase::default(),
        )
    }

    /// Returns the full lowercase mapping of the given string as a string.
    ///
    /// This function is context and language sensitive. Callers should pass the text's language
    /// as a `LanguageIdentifier` (usually the `id` field of the `Locale`) if available, or
    /// `Default::default()` for the root locale.
    ///
    /// See [`Self::lowercase()`] for the equivalent lower-level function that returns a [`Writeable`]
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::casemap::CaseMapper;
    /// use icu::locale::langid;
    ///
    /// let cm = CaseMapper::new();
    /// let root = langid!("und");
    ///
    /// assert_eq!(cm.lowercase_to_string("hEllO WorLd", &root), "hello world");
    /// assert_eq!(cm.lowercase_to_string("Γειά σου Κόσμε", &root), "γειά σου κόσμε");
    /// assert_eq!(cm.lowercase_to_string("नमस्ते दुनिया", &root), "नमस्ते दुनिया");
    /// assert_eq!(cm.lowercase_to_string("Привет мир", &root), "привет мир");
    ///
    /// // Some behavior is language-sensitive
    /// assert_eq!(cm.lowercase_to_string("CONSTANTINOPLE", &root), "constantinople");
    /// assert_eq!(cm.lowercase_to_string("CONSTANTINOPLE", &langid!("tr")), "constantınople");
    /// ```
    pub fn lowercase_to_string<'s>(
        self,
        src: &'s str,
        langid: &LanguageIdentifier,
    ) -> Cow<'s, str> {
        writeable::to_string_or_borrow(&self.lowercase(src, langid), src.as_bytes())
    }

    /// Returns the full uppercase mapping of the given string as a string.
    ///
    /// This function is context and language sensitive. Callers should pass the text's language
    /// as a `LanguageIdentifier` (usually the `id` field of the `Locale`) if available, or
    /// `Default::default()` for the root locale.
    ///
    /// See [`Self::uppercase()`] for the equivalent lower-level function that returns a [`Writeable`]
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::casemap::CaseMapper;
    /// use icu::locale::langid;
    ///
    /// let cm = CaseMapper::new();
    /// let root = langid!("und");
    ///
    /// assert_eq!(cm.uppercase_to_string("hEllO WorLd", &root), "HELLO WORLD");
    /// assert_eq!(cm.uppercase_to_string("Γειά σου Κόσμε", &root), "ΓΕΙΆ ΣΟΥ ΚΌΣΜΕ");
    /// assert_eq!(cm.uppercase_to_string("नमस्ते दुनिया", &root), "नमस्ते दुनिया");
    /// assert_eq!(cm.uppercase_to_string("Привет мир", &root), "ПРИВЕТ МИР");
    ///
    /// // Some behavior is language-sensitive
    /// assert_eq!(cm.uppercase_to_string("istanbul", &root), "ISTANBUL");
    /// assert_eq!(cm.uppercase_to_string("istanbul", &langid!("tr")), "İSTANBUL"); // Turkish dotted i
    ///
    /// assert_eq!(cm.uppercase_to_string("և Երևանի", &root), "ԵՒ ԵՐԵՒԱՆԻ");
    /// assert_eq!(cm.uppercase_to_string("և Երևանի", &langid!("hy")), "ԵՎ ԵՐԵՎԱՆԻ"); // Eastern Armenian ech-yiwn ligature
    /// ```
    pub fn uppercase_to_string<'s>(
        self,
        src: &'s str,
        langid: &LanguageIdentifier,
    ) -> Cow<'s, str> {
        writeable::to_string_or_borrow(&self.uppercase(src, langid), src.as_bytes())
    }

    /// Returns the full titlecase mapping of the given string as a [`Writeable`], treating
    /// the string as a single segment (and thus only titlecasing the beginning of it). Performs
    /// the specified leading adjustment behavior from the options without loading additional data.
    ///
    /// Note that [`TitlecaseMapper`] has better behavior, most users should consider using
    /// it instead. This method primarily exists for people who care about the amount of data being loaded.
    ///
    /// This should typically be used as a lower-level helper to construct the titlecasing operation desired
    /// by the application, for example one can titlecase on a per-word basis by mixing this with
    /// a `WordSegmenter`.
    ///
    /// This function is context and language sensitive. Callers should pass the text's language
    /// as a `LanguageIdentifier` (usually the `id` field of the `Locale`) if available, or
    /// `Default::default()` for the root locale.
    ///
    /// This function performs "adjust to cased" leading adjustment behavior when [`LeadingAdjustment::Auto`] or [`LeadingAdjustment::ToCased`]
    /// is set. Auto mode is not able to pick the "adjust to letter/number/symbol" behavior as this type does not load
    /// the data to do so, use [`TitlecaseMapper`] if such behavior is desired. See
    /// the docs of [`TitlecaseMapper`] for more information on what this means. There is no difference between
    /// the behavior of this function and the equivalent ones on [`TitlecaseMapper`] when the head adjustment mode
    /// is [`LeadingAdjustment::None`].
    ///
    /// See [`Self::titlecase_segment_with_only_case_data()`] for the equivalent lower-level function that returns a [`Writeable`]
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::casemap::CaseMapper;
    /// use icu::locale::langid;
    ///
    /// let cm = CaseMapper::new();
    /// let root = langid!("und");
    ///
    /// let default_options = Default::default();
    ///
    /// // note that the subsequent words are not titlecased, this function assumes
    /// // that the entire string is a single segment and only titlecases at the beginning.
    /// assert_eq!(cm.titlecase_segment_with_only_case_data_to_string("hEllO WorLd", &root, default_options), "Hello world");
    /// assert_eq!(cm.titlecase_segment_with_only_case_data_to_string("Γειά σου Κόσμε", &root, default_options), "Γειά σου κόσμε");
    /// assert_eq!(cm.titlecase_segment_with_only_case_data_to_string("नमस्ते दुनिया", &root, default_options), "नमस्ते दुनिया");
    /// assert_eq!(cm.titlecase_segment_with_only_case_data_to_string("Привет мир", &root, default_options), "Привет мир");
    ///
    /// // Some behavior is language-sensitive
    /// assert_eq!(cm.titlecase_segment_with_only_case_data_to_string("istanbul", &root, default_options), "Istanbul");
    /// assert_eq!(cm.titlecase_segment_with_only_case_data_to_string("istanbul", &langid!("tr"), default_options), "İstanbul"); // Turkish dotted i
    ///
    /// assert_eq!(cm.titlecase_segment_with_only_case_data_to_string("և Երևանի", &root, default_options), "Եւ երևանի");
    /// assert_eq!(cm.titlecase_segment_with_only_case_data_to_string("և Երևանի", &langid!("hy"), default_options), "Եվ երևանի"); // Eastern Armenian ech-yiwn ligature
    ///
    /// assert_eq!(cm.titlecase_segment_with_only_case_data_to_string("ijkdijk", &root, default_options), "Ijkdijk");
    /// assert_eq!(cm.titlecase_segment_with_only_case_data_to_string("ijkdijk", &langid!("nl"), default_options), "IJkdijk"); // Dutch IJ digraph
    /// ```
    ///
    /// [`TitlecaseMapper`]: crate::TitlecaseMapper
    pub fn titlecase_segment_with_only_case_data_to_string<'s>(
        self,
        src: &'s str,
        langid: &LanguageIdentifier,
        options: TitlecaseOptions,
    ) -> Cow<'s, str> {
        writeable::to_string_or_borrow(
            &self.titlecase_segment_with_only_case_data(src, langid, options),
            src.as_bytes(),
        )
    }

    /// Case-folds the characters in the given string as a String.
    /// This function is locale-independent and context-insensitive.
    ///
    /// Can be used to test if two strings are case-insensitively equivalent.
    ///
    /// See [`Self::fold()`] for the equivalent lower-level function that returns a [`Writeable`]
    ///s s
    /// # Examples
    ///
    /// ```rust
    /// use icu::casemap::CaseMapper;
    ///
    /// let cm = CaseMapper::new();
    ///
    /// // Check if two strings are equivalent case insensitively
    /// assert_eq!(cm.fold_string("hEllO WorLd"), cm.fold_string("HELLO worlD"));
    ///
    /// assert_eq!(cm.fold_string("hEllO WorLd"), "hello world");
    /// assert_eq!(cm.fold_string("Γειά σου Κόσμε"), "γειά σου κόσμε");
    /// assert_eq!(cm.fold_string("नमस्ते दुनिया"), "नमस्ते दुनिया");
    /// assert_eq!(cm.fold_string("Привет мир"), "привет мир");
    /// ```
    pub fn fold_string(self, src: &str) -> Cow<'_, str> {
        writeable::to_string_or_borrow(&self.fold(src), src.as_bytes())
    }

    /// Case-folds the characters in the given string as a String,
    /// using Turkic (T) mappings for dotted/dotless I.
    /// This function is locale-independent and context-insensitive.
    ///
    /// Can be used to test if two strings are case-insensitively equivalent.
    ///
    /// See [`Self::fold_turkic()`] for the equivalent lower-level function that returns a [`Writeable`]
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::casemap::CaseMapper;
    ///
    /// let cm = CaseMapper::new();
    ///
    /// // Check if two strings are equivalent case insensitively
    /// assert_eq!(cm.fold_turkic_string("İstanbul"), cm.fold_turkic_string("iSTANBUL"));
    ///
    /// assert_eq!(cm.fold_turkic_string("İstanbul not Constantinople"), "istanbul not constantinople");
    /// assert_eq!(cm.fold_turkic_string("Istanbul not Constantınople"), "ıstanbul not constantınople");
    ///
    /// assert_eq!(cm.fold_turkic_string("hEllO WorLd"), "hello world");
    /// assert_eq!(cm.fold_turkic_string("Γειά σου Κόσμε"), "γειά σου κόσμε");
    /// assert_eq!(cm.fold_turkic_string("नमस्ते दुनिया"), "नमस्ते दुनिया");
    /// assert_eq!(cm.fold_turkic_string("Привет мир"), "привет мир");
    /// ```
    pub fn fold_turkic_string(self, src: &str) -> Cow<'_, str> {
        writeable::to_string_or_borrow(&self.fold_turkic(src), src.as_bytes())
    }

    /// Adds all simple case mappings and the full case folding for `c` to `set`.
    /// Also adds special case closure mappings.
    ///
    /// Identical to [`CaseMapCloserBorrowed::add_case_closure_to()`], see docs there for more information.
    /// This method is duplicated so that one does not need to load extra unfold data
    /// if they only need this and not also [`CaseMapCloserBorrowed::add_string_case_closure_to()`].
    ///
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::casemap::CaseMapper;
    /// use icu::collections::codepointinvlist::CodePointInversionListBuilder;
    ///
    /// let cm = CaseMapper::new();
    /// let mut builder = CodePointInversionListBuilder::new();
    /// cm.add_case_closure_to('s', &mut builder);
    ///
    /// let set = builder.build();
    ///
    /// assert!(set.contains('S'));
    /// assert!(set.contains('ſ'));
    /// assert!(!set.contains('s')); // does not contain itself
    /// ```
    ///
    /// [`CaseMapCloserBorrowed::add_case_closure_to()`]: crate::CaseMapCloserBorrowed::add_case_closure_to
    /// [`CaseMapCloserBorrowed::add_string_case_closure_to()`]: crate::CaseMapCloserBorrowed::add_string_case_closure_to
    pub fn add_case_closure_to<S: ClosureSink>(self, c: char, set: &mut S) {
        self.data.add_case_closure_to(c, set);
    }

    /// Returns the lowercase mapping of the given `char`.
    /// This function only implements simple and common mappings. Full mappings,
    /// which can map one `char` to a string, are not included.
    /// For full mappings, use [`CaseMapperBorrowed::lowercase`].
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::casemap::CaseMapper;
    ///
    /// let cm = CaseMapper::new();
    ///
    /// assert_eq!(cm.simple_lowercase('C'), 'c');
    /// assert_eq!(cm.simple_lowercase('c'), 'c');
    /// assert_eq!(cm.simple_lowercase('Ć'), 'ć');
    /// assert_eq!(cm.simple_lowercase('Γ'), 'γ');
    /// ```
    pub fn simple_lowercase(self, c: char) -> char {
        self.data.simple_lower(c)
    }

    /// Returns the uppercase mapping of the given `char`.
    /// This function only implements simple and common mappings. Full mappings,
    /// which can map one `char` to a string, are not included.
    /// For full mappings, use [`CaseMapperBorrowed::uppercase`].
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::casemap::CaseMapper;
    ///
    /// let cm = CaseMapper::new();
    ///
    /// assert_eq!(cm.simple_uppercase('c'), 'C');
    /// assert_eq!(cm.simple_uppercase('C'), 'C');
    /// assert_eq!(cm.simple_uppercase('ć'), 'Ć');
    /// assert_eq!(cm.simple_uppercase('γ'), 'Γ');
    ///
    /// assert_eq!(cm.simple_uppercase('ǳ'), 'Ǳ');
    /// ```
    pub fn simple_uppercase(self, c: char) -> char {
        self.data.simple_upper(c)
    }

    /// Returns the titlecase mapping of the given `char`.
    /// This function only implements simple and common mappings. Full mappings,
    /// which can map one `char` to a string, are not included.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::casemap::CaseMapper;
    ///
    /// let cm = CaseMapper::new();
    ///
    /// assert_eq!(cm.simple_titlecase('ǳ'), 'ǲ');
    ///
    /// assert_eq!(cm.simple_titlecase('c'), 'C');
    /// assert_eq!(cm.simple_titlecase('C'), 'C');
    /// assert_eq!(cm.simple_titlecase('ć'), 'Ć');
    /// assert_eq!(cm.simple_titlecase('γ'), 'Γ');
    /// ```
    pub fn simple_titlecase(self, c: char) -> char {
        self.data.simple_title(c)
    }

    /// Returns the simple case folding of the given char.
    /// For full mappings, use [`CaseMapperBorrowed::fold`].
    ///
    /// This function can be used to perform caseless matches on
    /// individual characters.
    /// > *Note:* With Unicode 15.0 data, there are three
    /// > pairs of characters for which equivalence under this
    /// > function is inconsistent with equivalence of the
    /// > one-character strings under [`CaseMapperBorrowed::fold`].
    /// > This is resolved in Unicode 15.1 and later.
    ///
    /// For compatibility applications where simple case folding
    /// of strings is required, this function can be applied to
    /// each character of a string.  Note that the resulting
    /// equivalence relation is different from that obtained
    /// by [`CaseMapperBorrowed::fold`]:
    /// The strings "Straße" and "STRASSE" are distinct
    /// under simple case folding, but are equivalent under
    /// default (full) case folding.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::casemap::CaseMapper;
    ///
    /// let cm = CaseMapper::new();
    ///
    /// // perform case insensitive checks
    /// assert_eq!(cm.simple_fold('σ'), cm.simple_fold('ς'));
    /// assert_eq!(cm.simple_fold('Σ'), cm.simple_fold('ς'));
    ///
    /// assert_eq!(cm.simple_fold('c'), 'c');
    /// assert_eq!(cm.simple_fold('Ć'), 'ć');
    /// assert_eq!(cm.simple_fold('Γ'), 'γ');
    /// assert_eq!(cm.simple_fold('ς'), 'σ');
    ///
    /// assert_eq!(cm.simple_fold('ß'), 'ß');
    /// assert_eq!(cm.simple_fold('I'), 'i');
    /// assert_eq!(cm.simple_fold('İ'), 'İ');
    /// assert_eq!(cm.simple_fold('ı'), 'ı');
    /// ```
    pub fn simple_fold(self, c: char) -> char {
        self.data.simple_fold(c, FoldOptions::default())
    }

    /// Returns the simple case folding of the given char, using Turkic (T) mappings for
    /// dotted/dotless i. This function does not fold `i` and `I` to the same character. Instead,
    /// `I` will fold to `ı`, and `İ` will fold to `i`. Otherwise, this is the same as
    /// [`CaseMapperBorrowed::fold()`].
    ///
    /// You can use the case folding to perform Turkic caseless matches on characters
    /// provided they don't full-casefold to strings. To avoid that situation,
    /// convert to a string and use [`CaseMapperBorrowed::fold_turkic`].
    ///
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::casemap::CaseMapper;
    ///
    /// let cm = CaseMapper::new();
    ///
    /// assert_eq!(cm.simple_fold_turkic('I'), 'ı');
    /// assert_eq!(cm.simple_fold_turkic('İ'), 'i');
    /// ```
    pub fn simple_fold_turkic(self, c: char) -> char {
        self.data
            .simple_fold(c, FoldOptions::with_turkic_mappings())
    }
}

impl CaseMapper {
    /// Creates a [`CaseMapperBorrowed`] using compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::casemap::CaseMapper;
    /// use icu::locale::langid;
    ///
    /// let cm = CaseMapper::new();
    ///
    /// assert_eq!(
    ///     cm.uppercase_to_string("hello world", &langid!("und")),
    ///     "HELLO WORLD"
    /// );
    /// ```
    #[cfg(feature = "compiled_data")]
    #[expect(clippy::new_ret_no_self)] // Intentional
    pub const fn new() -> CaseMapperBorrowed<'static> {
        CaseMapperBorrowed::new()
    }

    /// Constructs a borrowed version of this type for more efficient querying.
    pub fn as_borrowed(&self) -> CaseMapperBorrowed<'_> {
        CaseMapperBorrowed {
            data: self.data.get(),
        }
    }

    icu_provider::gen_buffer_data_constructors!(() -> error: DataError,
    functions: [
        new: skip,
        try_new_with_buffer_provider,
        try_new_unstable,
        Self,
    ]);

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable<P>(provider: &P) -> Result<CaseMapper, DataError>
    where
        P: DataProvider<CaseMapV1> + ?Sized,
    {
        let data = provider.load(Default::default())?.payload;
        Ok(Self { data })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use icu_locale_core::langid;

    #[test]
    /// Tests for SpecialCasing.txt. Some of the special cases are data-driven, some are code-driven
    fn test_special_cases() {
        let cm = CaseMapper::new();
        let root = langid!("und");
        let default_options = Default::default();

        // Ligatures

        // U+FB00 LATIN SMALL LIGATURE FF
        assert_eq!(cm.uppercase_to_string("ﬀ", &root), "FF");
        // U+FB05 LATIN SMALL LIGATURE LONG S T
        assert_eq!(cm.uppercase_to_string("ﬅ", &root), "ST");

        // No corresponding uppercased character

        // U+0149 LATIN SMALL LETTER N PRECEDED BY APOSTROPHE
        assert_eq!(cm.uppercase_to_string("ŉ", &root), "ʼN");

        // U+1F50 GREEK SMALL LETTER UPSILON WITH PSILI
        assert_eq!(cm.uppercase_to_string("ὐ", &root), "Υ̓");
        // U+1FF6 GREEK SMALL LETTER OMEGA WITH PERISPOMENI
        assert_eq!(cm.uppercase_to_string("ῶ", &root), "Ω͂");

        // YPOGEGRAMMENI / PROSGEGRAMMENI special cases

        // E.g. <alpha><iota_subscript><acute> is uppercased to <ALPHA><acute><IOTA>
        assert_eq!(
            cm.uppercase_to_string("α\u{0313}\u{0345}", &root),
            "Α\u{0313}Ι"
        );
        // but the YPOGEGRAMMENI should not titlecase
        assert_eq!(
            cm.titlecase_segment_with_only_case_data_to_string(
                "α\u{0313}\u{0345}",
                &root,
                default_options
            ),
            "Α\u{0313}\u{0345}"
        );

        // U+1F80 GREEK SMALL LETTER ALPHA WITH PSILI AND YPOGEGRAMMENI
        assert_eq!(
            cm.titlecase_segment_with_only_case_data_to_string("ᾀ", &root, default_options),
            "ᾈ"
        );
        assert_eq!(cm.uppercase_to_string("ᾀ", &root), "ἈΙ");

        // U+1FFC GREEK CAPITAL LETTER OMEGA WITH PROSGEGRAMMENI
        assert_eq!(cm.lowercase_to_string("ῼ", &root), "ῳ");
        assert_eq!(
            cm.titlecase_segment_with_only_case_data_to_string("ῼ", &root, default_options),
            "ῼ"
        );
        assert_eq!(cm.uppercase_to_string("ῼ", &root), "ΩΙ");

        // U+1F98 GREEK CAPITAL LETTER ETA WITH PSILI AND PROSGEGRAMMENI
        assert_eq!(cm.lowercase_to_string("ᾘ", &root), "ᾐ");
        assert_eq!(
            cm.titlecase_segment_with_only_case_data_to_string("ᾘ", &root, default_options),
            "ᾘ"
        );
        assert_eq!(cm.uppercase_to_string("ᾘ", &root), "ἨΙ");

        // U+1FB2 GREEK SMALL LETTER ALPHA WITH VARIA AND YPOGEGRAMMENI
        assert_eq!(cm.lowercase_to_string("ᾲ", &root), "ᾲ");
        assert_eq!(
            cm.titlecase_segment_with_only_case_data_to_string("ᾲ", &root, default_options),
            "Ὰ\u{345}"
        );
        assert_eq!(cm.uppercase_to_string("ᾲ", &root), "ᾺΙ");

        // Final sigma test
        // U+03A3 GREEK CAPITAL LETTER SIGMA in Final_Sigma context
        assert_eq!(cm.lowercase_to_string("ΙΙΙΣ", &root), "ιιις");

        // Turkish / Azeri
        let tr = langid!("tr");
        let az = langid!("az");
        // U+0130 LATIN CAPITAL LETTER I WITH DOT ABOVE
        assert_eq!(cm.lowercase_to_string("İ", &tr), "i");
        assert_eq!(cm.lowercase_to_string("İ", &az), "i");
        assert_eq!(
            cm.titlecase_segment_with_only_case_data_to_string("İ", &tr, default_options),
            "İ"
        );
        assert_eq!(
            cm.titlecase_segment_with_only_case_data_to_string("İ", &az, default_options),
            "İ"
        );
        assert_eq!(cm.uppercase_to_string("İ", &tr), "İ");
        assert_eq!(cm.uppercase_to_string("İ", &az), "İ");

        // U+0049 LATIN CAPITAL LETTER I and U+0307 COMBINING DOT ABOVE
        assert_eq!(cm.lowercase_to_string("I\u{0307}", &tr), "i");
        assert_eq!(cm.lowercase_to_string("I\u{0307}", &az), "i");
        assert_eq!(
            cm.titlecase_segment_with_only_case_data_to_string("I\u{0307}", &tr, default_options),
            "I\u{0307}"
        );
        assert_eq!(
            cm.titlecase_segment_with_only_case_data_to_string("I\u{0307}", &az, default_options),
            "I\u{0307}"
        );
        assert_eq!(cm.uppercase_to_string("I\u{0307}", &tr), "I\u{0307}");
        assert_eq!(cm.uppercase_to_string("I\u{0307}", &az), "I\u{0307}");

        // U+0049 LATIN CAPITAL LETTER I
        assert_eq!(cm.lowercase_to_string("I", &tr), "ı");
        assert_eq!(cm.lowercase_to_string("I", &az), "ı");
        assert_eq!(
            cm.titlecase_segment_with_only_case_data_to_string("I", &tr, default_options),
            "I"
        );
        assert_eq!(
            cm.titlecase_segment_with_only_case_data_to_string("I", &az, default_options),
            "I"
        );
        assert_eq!(cm.uppercase_to_string("I", &tr), "I");
        assert_eq!(cm.uppercase_to_string("I", &az), "I");

        // U+0069 LATIN SMALL LETTER I
        assert_eq!(cm.lowercase_to_string("i", &tr), "i");
        assert_eq!(cm.lowercase_to_string("i", &az), "i");
        assert_eq!(
            cm.titlecase_segment_with_only_case_data_to_string("i", &tr, default_options),
            "İ"
        );
        assert_eq!(
            cm.titlecase_segment_with_only_case_data_to_string("i", &az, default_options),
            "İ"
        );
        assert_eq!(cm.uppercase_to_string("i", &tr), "İ");
        assert_eq!(cm.uppercase_to_string("i", &az), "İ");
    }

    #[test]
    fn test_cherokee_case_folding() {
        let case_mapping = CaseMapper::new();
        assert_eq!(case_mapping.simple_fold('Ꭰ'), 'Ꭰ');
        assert_eq!(case_mapping.simple_fold('ꭰ'), 'Ꭰ');
        assert_eq!(case_mapping.simple_fold_turkic('Ꭰ'), 'Ꭰ');
        assert_eq!(case_mapping.simple_fold_turkic('ꭰ'), 'Ꭰ');
        assert_eq!(case_mapping.fold_string("Ꭰ"), "Ꭰ");
        assert_eq!(case_mapping.fold_string("ꭰ"), "Ꭰ");
        assert_eq!(case_mapping.fold_turkic_string("Ꭰ"), "Ꭰ");
        assert_eq!(case_mapping.fold_turkic_string("ꭰ"), "Ꭰ");
    }
}
