// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![expect(clippy::indexing_slicing, clippy::unwrap_used)] // TODO(#3958): Remove.

mod hardcoded;
mod replaceable;

use crate::transliterate::provider::{FunctionCall, Rule, RuleULE, SimpleId, VarTable};
use crate::transliterate::provider::{RuleBasedTransliterator, Segment, TransliteratorRulesV1};
use crate::transliterate::transliterator::hardcoded::Case;
use alloc::borrow::Cow;
use alloc::boxed::Box;
use alloc::string::{String, ToString};
use alloc::vec::Vec;
use core::fmt::Debug;
use core::ops::Range;
use icu_casemap::provider::CaseMapV1;
use icu_casemap::CaseMapper;
use icu_collections::codepointinvlist::CodePointInversionList;
use icu_collections::codepointinvliststringlist::CodePointInversionListAndStringList;
use icu_locale::LanguageIdentifier;
use icu_locale_core::Locale;
use icu_normalizer::provider::*;
use icu_normalizer::{ComposingNormalizer, DecomposingNormalizer};
use icu_provider::prelude::*;
use litemap::LiteMap;
use replaceable::*;
use zerofrom::ZeroFrom;
use zerovec::vecs::Index32;
use zerovec::VarZeroSlice;

type Filter<'a> = CodePointInversionList<'a>;

// Thought: How about a RunTransliterator trait that is implemented for all internal types, is blanket
//  implemented for CustomTransliterator, and maybe is exposed to users if they want more control?
//  Actually, an alternative would be: CustomTransliterator is just &str -> String, RunTransliterator is
//  (&str, allowed_range) -> String, and some RepTransliterator would just be Replaceable -> ().

/// A type that supports transliteration. Used for overrides in [`Transliterator`] - see
/// [`Transliterator::try_new_with_override_unstable`].
pub trait CustomTransliterator: Debug {
    /// Transliterates the portion of the input string specified by the byte indices in the range.
    ///
    /// The returned `String` must just be the transliteration of `input[range]`. The rest is
    /// there for context, if necessary.
    fn transliterate(&self, input: &str, range: Range<usize>) -> String;
}

#[derive(Debug)]
enum InternalTransliterator {
    RuleBased(DataPayload<TransliteratorRulesV1>),
    Composing(ComposingNormalizer),
    Decomposing(DecomposingNormalizer),
    Hex(hardcoded::HexTransliterator),
    Lower(CaseMapper),
    Upper(CaseMapper),
    Null,
    Remove,
    Dyn(Box<dyn CustomTransliterator>),
}

impl InternalTransliterator {
    fn transliterate(&self, mut rep: Replaceable, env: &Env) {
        match self {
            Self::RuleBased(rbt) => rbt.get().transliterate(rep, env),
            // TODO(#3910): internal hardcoded transliterators
            Self::Composing(normalizer) => {
                if let Cow::Owned(buf) = normalizer.as_borrowed().normalize(rep.as_str_modifiable())
                {
                    rep.replace_modifiable_with_str(&buf);
                }
            }
            Self::Decomposing(normalizer) => {
                if let Cow::Owned(buf) = normalizer.as_borrowed().normalize(rep.as_str_modifiable())
                {
                    rep.replace_modifiable_with_str(&buf);
                }
            }
            Self::Lower(casemap) => {
                if let Cow::Owned(buf) = casemap
                    .as_borrowed()
                    .lowercase_to_string(rep.as_str_modifiable(), &LanguageIdentifier::UNKNOWN)
                {
                    rep.replace_modifiable_with_str(&buf);
                }
            }
            Self::Upper(casemap) => {
                if let Cow::Owned(buf) = casemap
                    .as_borrowed()
                    .uppercase_to_string(rep.as_str_modifiable(), &LanguageIdentifier::UNKNOWN)
                {
                    rep.replace_modifiable_with_str(&buf);
                }
            }
            Self::Hex(t) => t.transliterate(rep),
            Self::Null => (),
            Self::Remove => rep.replace_modifiable_with_str(""),
            Self::Dyn(custom) => {
                let replacement = custom.transliterate(rep.as_str(), rep.allowed_range());
                rep.replace_modifiable_with_str(&replacement)
            }
        }
    }
}

type Env = LiteMap<String, InternalTransliterator>;

/// A `Transliterator` allows transliteration based on [UTS #35 transform rules](https://unicode.org/reports/tr35/tr35-general.html#Transforms),
/// including overrides with custom implementations.
///
/// # Examples
///
/// A transliterator with a custom alias referenced by another:
///
/// ```
/// use icu::experimental::transliterate::{Transliterator, CustomTransliterator, RuleCollection};
/// use icu::locale::Locale;
///
/// // Set up a transliterator with 3 custom rules.
/// // Note: These rules are for demonstration purposes only! Do not use.
///
/// // 1. Main entrypoint: a chain of several transliterators
/// let mut collection = RuleCollection::default();
/// collection.register_source(
///     &"und-t-und-x0-custom".parse().unwrap(),
///     "::NFD; ::FlattenLowerUmlaut; ::[:Nonspacing_Mark:] Remove; ::AsciiUpper; ::NFC;".to_string(),
///     [],
///     false,
///     true,
/// );
///
/// // 2. A custom ruleset that expands lowercase umlauts
/// collection.register_source(
///     &"und-t-und-x0-dep1".parse().unwrap(),
///     r#"
///       [ä {a \u0308}] → ae;
///       [ö {o \u0308}] → oe;
///       [ü {u \u0308}] → ue;
///     "#.to_string(),
///     ["Any-FlattenLowerUmlaut"],
///     false,
///     true,
/// );
///
/// // 3. A custom transliterator that uppercases all ASCII characters
/// #[derive(Debug)]
/// struct AsciiUpperTransliterator;
/// impl CustomTransliterator for AsciiUpperTransliterator {
///     fn transliterate(&self, input: &str, range: core::ops::Range<usize>) -> String {
///         input.to_ascii_uppercase()
///     }
/// }
/// collection.register_aliases(
///     &"und-t-und-x0-dep2".parse().unwrap(),
///     ["Any-AsciiUpper"],
/// );
///
/// // Create a transliterator from the main entrypoint:
/// let provider = collection.as_provider();
/// let t = Transliterator::try_new_with_override_unstable(
///     &provider,
///     &provider,
///     &provider,
///     &"und-t-und-x0-custom".parse().unwrap(),
///     |locale| locale.normalizing_eq("und-t-und-x0-dep2").then_some(Ok(Box::new(AsciiUpperTransliterator))),
/// )
/// .unwrap();
///
/// // Test the behavior:
/// // - The uppercase 'Ü' is stripped of its umlaut
/// // - The lowercase 'ä' is expanded to "ae"
/// // - All ASCII characters are uppercased: not 'ß', which is not ASCII
/// let r = t.transliterate("Übermäßig".to_string());
/// assert_eq!(r, "UBERMAEßIG");
/// ```
#[derive(Debug)]
pub struct Transliterator {
    transliterator: DataPayload<TransliteratorRulesV1>,
    env: Env,
}

/// Builder type for [`Transliterator`]
#[derive(Debug)]
#[cfg(feature = "compiled_data")]
pub struct TransliteratorBuilder {
    env: Env,
    transliterator: DataPayload<TransliteratorRulesV1>,
}

#[cfg(feature = "compiled_data")]
impl Default for TransliteratorBuilder {
    fn default() -> Self {
        Self {
            env: LiteMap::from_iter([
                ("any-remove".into(), InternalTransliterator::Remove),
                ("any-null".into(), InternalTransliterator::Null),
            ]),
            transliterator: DataPayload::from_owned(RuleBasedTransliterator {
                visibility: false,
                variable_table: Default::default(),
                filter: CodePointInversionList::all(),
                id_group_list: Default::default(),
                rule_group_list: Default::default(),
            }),
        }
    }
}

#[cfg(feature = "compiled_data")]
impl TransliteratorBuilder {
    /// Creates a [`TransliteratorBuilder`] from a baked data struct.
    ///
    /// This method can be used to statically construct a [`Transliterator`] without including
    /// all transliterators (which [`Transliterator::try_new`] does).
    ///
    /// Warning: adding additional rules after using this constructor will allocate `rules`.
    /// If you need to add more rules, prefer using [`TransliteratorBuilder::default()`] and
    /// [`TransliteratorBuilder::call`].
    pub fn from_rules(rules: &'static RuleBasedTransliterator<'static>) -> Self {
        Self {
            transliterator: DataPayload::from_static_ref(rules),
            ..Default::default()
        }
    }

    /// Adds a replacement rule, replacing all strings in `matcher` by `replacer`.
    pub fn replace(
        mut self,
        matcher: CodePointInversionListAndStringList<'static>,
        replacer: String,
    ) -> Self {
        if matcher.size() == 0 {
            return self;
        }
        self.transliterator.with_mut(move |r| {
            let rule_group_list = r.rule_group_list.make_mut();

            let mut group = if rule_group_list.is_empty() {
                Default::default()
            } else {
                let g = rule_group_list
                    .get(rule_group_list.len() - 1)
                    .unwrap()
                    .as_varzerovec()
                    .into_owned();
                rule_group_list.remove(rule_group_list.len() - 1);
                g
            };
            group.make_mut().push(&Rule {
                key: Cow::Owned(String::from(
                    // We can just use the index in the unicode_sets list because that's the only
                    // part of the variable table we use. If we used any of the other fields,
                    // we'd have to update all rules that use those indices on every insertion,
                    // as an insertion pushes the following indices up by one.
                    char::from_u32(
                        VarTable::BASE as u32 + r.variable_table.unicode_sets.len() as u32,
                    )
                    .unwrap(),
                )),
                replacer: Cow::Owned(replacer),
                ante: Cow::Borrowed(""),
                post: Cow::Borrowed(""),
            });
            rule_group_list.push(&group);

            r.variable_table.unicode_sets.make_mut().push(&matcher);
        });

        self
    }

    /// Adds a `::NFC` rule
    pub fn nfc(mut self, filter: CodePointInversionList<'static>) -> Self {
        if filter.is_empty() {
            return self;
        }
        self.chain(filter, Cow::Borrowed("any-nfc"));
        self.load_nfc()
    }

    /// Adds a `::NFKC` rule
    pub fn nfkc(mut self, filter: CodePointInversionList<'static>) -> Self {
        if filter.is_empty() {
            return self;
        }
        self.chain(filter, Cow::Borrowed("any-nfkc"));
        self.load_nfkc()
    }

    /// Adds a `::NFD` rule
    pub fn nfd(mut self, filter: CodePointInversionList<'static>) -> Self {
        if filter.is_empty() {
            return self;
        }
        self.chain(filter, Cow::Borrowed("any-nfd"));
        self.load_nfd()
    }

    /// Adds a `::NFKD` rule
    pub fn nfkd(mut self, filter: CodePointInversionList<'static>) -> Self {
        if filter.is_empty() {
            return self;
        }
        self.chain(filter, Cow::Borrowed("any-nfkd"));
        self.load_nfkd()
    }

    /// Adds a `::Lower` rule
    pub fn lower(mut self, filter: CodePointInversionList<'static>) -> Self {
        if filter.is_empty() {
            return self;
        }
        self.chain(filter, Cow::Borrowed("any-lower"));
        self.load_casing()
    }

    /// Adds a `::Upper` rule
    pub fn upper(mut self, filter: CodePointInversionList<'static>) -> Self {
        if filter.is_empty() {
            return self;
        }
        self.chain(filter, Cow::Borrowed("any-upper"));
        self.load_casing()
    }

    /// Adds a `::Remove` rule
    pub fn remove(mut self, filter: CodePointInversionList<'static>) -> Self {
        if filter.is_empty() {
            return self;
        }
        self.chain(filter, Cow::Borrowed("any-remove"));
        self
    }

    /// Adds a `::Null` rule
    pub fn null(mut self) -> Self {
        self.transliterator.with_mut(|r| {
            r.id_group_list
                .make_mut()
                .push::<&[SimpleId]>(&[].as_slice());

            r.rule_group_list.make_mut().push::<&[Rule]>(&[].as_slice());
        });

        self
    }

    /// Adds a call to another transliterator
    pub fn call(
        mut self,
        rules: &'static RuleBasedTransliterator<'static>,
        filter: CodePointInversionList<'static>,
    ) -> Self {
        if filter.is_empty() {
            return self;
        }
        let id = self.env.len().to_string();

        self.env.insert(
            id.clone(),
            InternalTransliterator::RuleBased(DataPayload::from_static_ref(rules)),
        );

        self.chain(filter, Cow::Owned(id));

        self
    }

    fn chain(&mut self, filter: CodePointInversionList<'static>, id: Cow<'static, str>) {
        self.transliterator.with_mut(|r| {
            r.id_group_list
                .make_mut()
                .push(&[SimpleId { filter, id }].as_slice());

            r.rule_group_list.make_mut().push::<&[Rule]>(&[].as_slice());
        });
    }

    /// Builds the transliterator.
    ///
    /// This method fails if a recursive dependency has not been loaded. Methods that add rules, such as
    /// [`Self::nfc`] load NFC data implicitly, but if this builder was constructed with [`Self::from_rules`] or
    /// calls a transliterator using [`Self::call`], all dependencies for the recursive transliterator need to
    /// have been loaded.
    pub fn build(self) -> Result<Transliterator, DataError> {
        for dep in self.transliterator.get().deps() {
            if !self.env.contains_key(&*dep) {
                return Err(DataError::custom("dependency not loaded").with_display_context(&dep));
            }
        }

        for (_, dep) in &self.env {
            if let InternalTransliterator::RuleBased(rbt) = dep {
                for dep in rbt.get().deps() {
                    if !self.env.contains_key(&*dep) {
                        return Err(
                            DataError::custom("dependency not loaded").with_display_context(&dep)
                        );
                    }
                }
            }
        }
        Ok(Transliterator {
            transliterator: self.transliterator,
            env: self.env,
        })
    }

    /// Loads NFC data. Call this if you load rules that use `::NFC`.
    pub fn load_nfc(mut self) -> Self {
        if !self.env.contains_key("any-nfc") {
            self.env.insert(
                String::from("any-nfc"),
                InternalTransliterator::Composing(ComposingNormalizer::new_nfc().static_to_owned()),
            );
        }

        self
    }

    /// Loads NFKC data. Call this if you load rules that use `::NFKC`.
    pub fn load_nfkc(mut self) -> Self {
        if !self.env.contains_key("any-nfkc") {
            self.env.insert(
                String::from("any-nfkc"),
                InternalTransliterator::Composing(
                    ComposingNormalizer::new_nfkc().static_to_owned(),
                ),
            );
        }

        self
    }

    /// Loads NFD data. Call this if you load rules that use `::NFD`.
    pub fn load_nfd(mut self) -> Self {
        if !self.env.contains_key("any-nfd") {
            self.env.insert(
                String::from("any-nfd"),
                InternalTransliterator::Decomposing(
                    DecomposingNormalizer::new_nfd().static_to_owned(),
                ),
            );
        }

        self
    }

    /// Loads NFKD data. Call this if you load rules that use `::NFKD`.
    pub fn load_nfkd(mut self) -> Self {
        if !self.env.contains_key("any-nfkd") {
            self.env.insert(
                String::from("any-nfkd"),
                InternalTransliterator::Decomposing(
                    DecomposingNormalizer::new_nfkd().static_to_owned(),
                ),
            );
        }

        self
    }

    /// Loads casing data. Call this if you load rules that use `::Lower` or `::Upper`.
    pub fn load_casing(mut self) -> Self {
        if !self.env.contains_key("any-lower") {
            self.env.insert(
                String::from("any-lower"),
                InternalTransliterator::Lower(CaseMapper::new().static_to_owned()),
            );
            self.env.insert(
                String::from("any-upper"),
                InternalTransliterator::Upper(CaseMapper::new().static_to_owned()),
            );
        }

        self
    }
}

impl Transliterator {
    /// Construct a [`Transliterator`] from the given [`Locale`].
    ///
    /// # Examples
    /// ```
    /// use icu::experimental::transliterate::Transliterator;
    /// // BCP-47-T ID for Bengali to Arabic transliteration
    /// let locale = "und-Arab-t-und-beng".parse().unwrap();
    /// let t = Transliterator::try_new(&locale).unwrap();
    /// let output = t.transliterate("অকার্যতানাযা".to_string());
    ///
    /// assert_eq!(output, "اكاريتانايا");
    /// ```
    #[cfg(feature = "compiled_data")]
    #[allow(unused_qualifications)]
    pub fn try_new(locale: &Locale) -> Result<Self, DataError> {
        Self::try_new_unstable(
            &crate::provider::Baked,
            &icu_normalizer::provider::Baked,
            &icu_casemap::provider::Baked,
            locale,
        )
    }

    #[cfg(feature = "serde")]
    #[doc = icu_provider::gen_buffer_unstable_docs!(BUFFER, Self::try_new)]
    pub fn try_new_with_buffer_provider(
        provider: &(impl BufferProvider + ?Sized),
        locale: &Locale,
    ) -> Result<Self, DataError> {
        Self::try_new_unstable(
            &provider.as_deserializing(),
            &provider.as_deserializing(),
            &provider.as_deserializing(),
            locale,
        )
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new)]
    pub fn try_new_unstable<PT, PN, PC>(
        transliterator_provider: &PT,
        normalizer_provider: &PN,
        casemap_provider: &PC,
        locale: &Locale,
    ) -> Result<Self, DataError>
    where
        PT: DataProvider<TransliteratorRulesV1> + ?Sized,
        PC: DataProvider<CaseMapV1> + ?Sized,
        PN: DataProvider<NormalizerNfdDataV1>
            + DataProvider<NormalizerNfkdDataV1>
            + DataProvider<NormalizerNfdTablesV1>
            + DataProvider<NormalizerNfkdTablesV1>
            + DataProvider<NormalizerNfcV1>
            + ?Sized,
    {
        Self::internal_try_new_with_override_unstable(
            locale,
            None::<&fn(&Locale) -> Option<Result<Box<dyn CustomTransliterator>, DataError>>>,
            transliterator_provider,
            normalizer_provider,
            casemap_provider,
        )
    }

    /// Construct a [`Transliterator`] from the given [`Locale`] using overrides provided
    /// by `lookup`.
    ///
    /// This allows clients to override the nested transliterators used by this transliterator.
    /// Any nested transliterator will first try to be loaded with `lookup`, and only fall back
    /// to the nested transliterator defined by the data if it returns `None`.
    /// See [`CustomTransliterator`].
    ///
    /// # Example
    /// Overriding `"de-t-de-d0-ascii"`'s dependency on `"und-t-und-Latn-d0-ascii"`:
    /// ```
    /// use core::ops::Range;
    /// use icu::experimental::transliterate::{
    ///     CustomTransliterator, Transliterator,
    /// };
    /// use icu::locale::Locale;
    ///
    /// #[derive(Debug)]
    /// struct FunkyGermanToAscii;
    /// impl CustomTransliterator for FunkyGermanToAscii {
    ///     fn transliterate(
    ///         &self,
    ///         input: &str,
    ///         allowed_range: Range<usize>,
    ///     ) -> String {
    ///         input[allowed_range].replace("oeverride", "overridden")
    ///     }
    /// }
    ///
    /// let override_locale: Locale = "und-t-und-Latn-d0-ascii".parse().unwrap();
    /// let locale = "de-t-de-d0-ascii".parse().unwrap();
    /// let t = Transliterator::try_new_with_override(&locale, |locale| {
    ///     override_locale
    ///         .eq(locale)
    ///         .then_some(Ok(Box::new(FunkyGermanToAscii)))
    /// })
    /// .unwrap();
    /// let output = t.transliterate("This is an överride example".to_string());
    ///
    /// assert_eq!(output, "This is an overridden example");
    /// ```
    #[cfg(feature = "compiled_data")]
    #[allow(unused_qualifications)]
    pub fn try_new_with_override<F>(locale: &Locale, lookup: F) -> Result<Self, DataError>
    where
        F: Fn(&Locale) -> Option<Result<Box<dyn CustomTransliterator>, DataError>>,
    {
        Self::try_new_with_override_unstable(
            &crate::provider::Baked,
            &icu_normalizer::provider::Baked,
            &icu_casemap::provider::Baked,
            locale,
            lookup,
        )
    }

    #[cfg(feature = "serde")]
    #[doc = icu_provider::gen_buffer_unstable_docs!(BUFFER, Self::try_new_with_override)]
    pub fn try_new_with_override_with_buffer_provider<F>(
        provider: &(impl BufferProvider + ?Sized),
        locale: &Locale,
        lookup: F,
    ) -> Result<Self, DataError>
    where
        F: Fn(&Locale) -> Option<Result<Box<dyn CustomTransliterator>, DataError>>,
    {
        Self::try_new_with_override_unstable(
            &provider.as_deserializing(),
            &provider.as_deserializing(),
            &provider.as_deserializing(),
            locale,
            lookup,
        )
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::try_new_with_override)]
    pub fn try_new_with_override_unstable<PT, PN, PC, F>(
        transliterator_provider: &PT,
        normalizer_provider: &PN,
        casemap_provider: &PC,
        locale: &Locale,
        lookup: F,
    ) -> Result<Transliterator, DataError>
    where
        PT: DataProvider<TransliteratorRulesV1> + ?Sized,
        PC: DataProvider<CaseMapV1> + ?Sized,
        PN: DataProvider<NormalizerNfdDataV1>
            + DataProvider<NormalizerNfkdDataV1>
            + DataProvider<NormalizerNfdTablesV1>
            + DataProvider<NormalizerNfkdTablesV1>
            + DataProvider<NormalizerNfcV1>
            + ?Sized,
        F: Fn(&Locale) -> Option<Result<Box<dyn CustomTransliterator>, DataError>>,
    {
        Self::internal_try_new_with_override_unstable(
            locale,
            Some(&lookup),
            transliterator_provider,
            normalizer_provider,
            casemap_provider,
        )
    }

    fn internal_try_new_with_override_unstable<PN, PT, PC, F>(
        locale: &Locale,
        lookup: Option<&F>,
        transliterator_provider: &PT,
        normalizer_provider: &PN,
        casemap_provider: &PC,
    ) -> Result<Transliterator, DataError>
    where
        PT: DataProvider<TransliteratorRulesV1> + ?Sized,
        PC: DataProvider<CaseMapV1> + ?Sized,
        PN: DataProvider<NormalizerNfdDataV1>
            + DataProvider<NormalizerNfkdDataV1>
            + DataProvider<NormalizerNfdTablesV1>
            + DataProvider<NormalizerNfkdTablesV1>
            + DataProvider<NormalizerNfcV1>
            + ?Sized,
        F: Fn(&Locale) -> Option<Result<Box<dyn CustomTransliterator>, DataError>>,
    {
        let mut env = LiteMap::new();

        let transliterator = Transliterator::load_rbt(
            #[expect(clippy::unwrap_used)] // infallible
            DataMarkerAttributes::try_from_str(&locale.to_string().to_ascii_lowercase()).unwrap(),
            lookup,
            transliterator_provider,
            normalizer_provider,
            casemap_provider,
            false,
            &mut env,
        )?;

        Ok(Transliterator {
            transliterator,
            env,
        })
    }

    fn load_rbt<PT, PN, PC, F>(
        marker_attributes: &DataMarkerAttributes,
        lookup: Option<&F>,
        transliterator_provider: &PT,
        normalizer_provider: &PN,
        casemap_provider: &PC,
        allow_internal: bool,
        env: &mut LiteMap<String, InternalTransliterator>,
    ) -> Result<DataPayload<TransliteratorRulesV1>, DataError>
    where
        PT: DataProvider<TransliteratorRulesV1> + ?Sized,
        PC: DataProvider<CaseMapV1> + ?Sized,
        PN: DataProvider<NormalizerNfdDataV1>
            + DataProvider<NormalizerNfkdDataV1>
            + DataProvider<NormalizerNfdTablesV1>
            + DataProvider<NormalizerNfkdTablesV1>
            + DataProvider<NormalizerNfcV1>
            + ?Sized,
        F: Fn(&Locale) -> Option<Result<Box<dyn CustomTransliterator>, DataError>>,
    {
        let req = DataRequest {
            id: DataIdentifierBorrowed::for_marker_attributes(marker_attributes),
            ..Default::default()
        };
        let transliterator = transliterator_provider.load(req)?.payload;
        if !allow_internal && !transliterator.get().visibility {
            return Err(DataError::custom("internal only transliterator"));
        }
        // Avoid recursive load
        env.insert(marker_attributes.to_string(), InternalTransliterator::Null);
        for dep in transliterator.get().deps() {
            if !env.contains_key(&*dep) {
                // Load the transliterator, by checking
                let internal_t =
                    // a) hardcoded specials
                    Transliterator::load_special(&dep, normalizer_provider, casemap_provider)
                    // b) the user-provided override
                    .or_else(|| Some(lookup?(&dep.parse().ok()?)?.map(InternalTransliterator::Dyn)))
                    // c) the data
                    .unwrap_or_else(|| {
                        Transliterator::load_rbt(
                            #[expect(clippy::unwrap_used)] // infallible
                            DataMarkerAttributes::try_from_str(&dep.to_ascii_lowercase()).unwrap(),
                            lookup,
                            transliterator_provider,
                            normalizer_provider,
                            casemap_provider,
                            true,
                            env,
                        ).map(InternalTransliterator::RuleBased)
                    })?;
                env.insert(dep.to_string(), internal_t);
            }
        }
        Ok(transliterator)
    }

    fn load_special<PN, PD>(
        special: &str,
        normalizer_provider: &PN,
        casemapper_provider: &PD,
    ) -> Option<Result<InternalTransliterator, DataError>>
    where
        PN: ?Sized
            + DataProvider<NormalizerNfdDataV1>
            + DataProvider<NormalizerNfkdDataV1>
            + DataProvider<NormalizerNfdTablesV1>
            + DataProvider<NormalizerNfkdTablesV1>
            + DataProvider<NormalizerNfcV1>,
        PD: ?Sized + DataProvider<CaseMapV1>,
    {
        // TODO(#3909, #3910): add more
        match special {
            "any-nfc" => Some(
                ComposingNormalizer::try_new_nfc_unstable(normalizer_provider)
                    .map(InternalTransliterator::Composing),
            ),
            "any-nfkc" => Some(
                ComposingNormalizer::try_new_nfkc_unstable(normalizer_provider)
                    .map(InternalTransliterator::Composing),
            ),
            "any-nfd" => Some(
                DecomposingNormalizer::try_new_nfd_unstable(normalizer_provider)
                    .map(InternalTransliterator::Decomposing),
            ),
            "any-nfkd" => Some(
                DecomposingNormalizer::try_new_nfkd_unstable(normalizer_provider)
                    .map(InternalTransliterator::Decomposing),
            ),
            "any-lower" => Some(
                CaseMapper::try_new_unstable(casemapper_provider)
                    .map(InternalTransliterator::Lower),
            ),
            "any-upper" => Some(
                CaseMapper::try_new_unstable(casemapper_provider)
                    .map(InternalTransliterator::Upper),
            ),
            "any-null" => Some(Ok(InternalTransliterator::Null)),
            "any-remove" => Some(Ok(InternalTransliterator::Remove)),
            "any-hex/unicode" => Some(Ok(InternalTransliterator::Hex(
                hardcoded::HexTransliterator::new("U+", "", 4, Case::Upper),
            ))),
            "any-hex/rust" => Some(Ok(InternalTransliterator::Hex(
                hardcoded::HexTransliterator::new("\\u{", "}", 2, Case::Lower),
            ))),
            "any-hex/xml" => Some(Ok(InternalTransliterator::Hex(
                hardcoded::HexTransliterator::new("&#x", ";", 1, Case::Upper),
            ))),
            "any-hex/perl" => Some(Ok(InternalTransliterator::Hex(
                hardcoded::HexTransliterator::new("\\x{", "}", 1, Case::Upper),
            ))),
            "any-hex/plain" => Some(Ok(InternalTransliterator::Hex(
                hardcoded::HexTransliterator::new("", "", 4, Case::Upper),
            ))),
            _ => None,
        }
    }

    // Before stabilization, consider the input type we want to accept here. We might want to
    // use a data structure internally that works nicely with a &str, but if we don't, a String
    // is good to accept because the user might already have one.
    /// Transliterates `input` and returns its transliteration.
    pub fn transliterate(&self, input: String) -> String {
        // Thought: Seems too much work for the benefits, but maybe have a Cow buffer instead?
        //  Insertable would only actually to_owned if the replaced bytes differ from the ones already there
        let mut buffer = TransliteratorBuffer::from_string(input);
        let rep = Replaceable::new(&mut buffer);
        self.transliterator.get().transliterate(rep, &self.env);
        buffer.into_string()
    }
}

impl RuleBasedTransliterator<'_> {
    /// Transliteration using rules works as follows:
    /// 1. Split the input modifiable range of the Replaceable according into runs according to self.filter
    /// 2. Transliterate each run in sequence
    ///     1. Transliterate the first `id_group`, then the first `rule_group`, then the second `id_group`, etc.
    fn transliterate(&self, mut rep: Replaceable, env: &Env) {
        // assumes the cursor is at the right position.

        rep.for_each_run(&self.filter, |run| {
            // eprintln!("got RBT filtered_run: {run:?}");
            for (id_group, rule_group) in self.id_group_list.iter().zip(self.rule_group_list.iter())
            {
                // first handle id_group
                for single_id in id_group.iter() {
                    let id = SimpleId::zero_from(single_id);
                    id.transliterate(run.child(), env);
                }

                // then handle rule_group
                let rule_group = RuleGroup::from(rule_group);
                rule_group.transliterate(run.child(), &self.variable_table, env);
            }
            // eprintln!("finished RBT filtered_run transliteration: {run:?}")
        });
    }
}

impl SimpleId<'_> {
    fn transliterate(&self, mut rep: Replaceable, env: &Env) {
        // eprintln!("transliterating SimpleId: {self:?}");
        // definitely loaded in the constructor
        let inner = env.get(self.id.as_ref()).unwrap();
        rep.for_each_run(&self.filter, |run| {
            // eprintln!("transliterating SimpleId run: {rep:?}");
            inner.transliterate(run.child(), env)
        })
    }
}

struct RuleGroup<'a> {
    rules: &'a VarZeroSlice<RuleULE, Index32>,
}

impl<'a> RuleGroup<'a> {
    fn from(rules: &'a VarZeroSlice<RuleULE, Index32>) -> Self {
        Self { rules }
    }

    fn transliterate(&self, mut rep: Replaceable, vt: &VarTable, env: &Env) {
        // no need to split into runs, because a RuleGroup has no filters.

        if self.rules.is_empty() {
            // empty rule group, nothing to do
            return;
        }

        // while the cursor has not reached the end yet, keep trying to apply each rule in order.
        // when a rule matches, apply its replacement and move the cursor according to the replacement

        // note that we're stopping transliteration at the end _even though empty rules might still match_.
        // this behavior is copied from ICU4C/J.
        'main: while !rep.is_finished() {
            // eprintln!("ongoing RuleGroup transliteration:\n{rep:?}");
            for rule in self.rules.iter() {
                let rule: Rule = Rule::zero_from(rule);
                // eprintln!("trying rule: {rule:?}");
                let matcher = rep.start_match();
                if let Some((data, matcher)) = rule.matches(matcher, vt) {
                    rule.apply(matcher.finish_match(), data, vt, env);
                    // eprintln!("finished applying replacement: {rep:?}");
                    // eprintln!("applied rule!");
                    // rule application is responsible for updating the cursor
                    continue 'main;
                }
            }
            // eprintln!("no rule matched, moving cursor forward");
            // no rule matched, so just move the cursor forward by one code point
            rep.step_cursor();
        }
    }
}

impl Rule<'_> {
    /// Applies this rule's replacement using the given [`MatchData`]. Updates the cursor of the
    /// current run.
    fn apply(&self, mut dest: Insertable, data: MatchData, vt: &VarTable, env: &Env) {
        // note: this could be precomputed ignoring segments and function calls.
        // A `rule.used_segments` ZeroVec<u16> could be added at compile time,
        // which would make it easier to take segments into account at runtime.
        // there is no easy way to estimate the size of function calls, though.
        // TODO(#3957): benchmark with and without this optimization
        let replacement_size_estimate = estimate_replacement_size(&self.replacer, &data, vt);

        dest.apply_size_hint(replacement_size_estimate);

        replace_str_with_specials(&self.replacer, &mut dest, &data, vt, env);
    }

    /// Returns `None` if there is no match. If there is a match, returns the associated
    /// [`MatchData`] and [`RepMatcher`].
    // Thought: RepMatcher<true> could be "FinishedRepMatcher"? but we can still match post..
    fn matches<'r1, 'r2>(
        &self,
        mut matcher: RepMatcher<'r1, 'r2, false>,
        vt: &VarTable,
    ) -> Option<(MatchData, RepMatcher<'r1, 'r2, true>)> {
        let mut match_data = MatchData::new();

        if !self.ante_matches(&mut matcher, &mut match_data, vt) {
            return None;
        }

        if !self.key_matches(&mut matcher, &mut match_data, vt) {
            return None;
        }

        let mut matcher = matcher.finish_key();

        if !self.post_matches(&mut matcher, &mut match_data, vt) {
            return None;
        }

        Some((match_data, matcher))
    }

    /// Returns whether the ante context matches or not. Fills in `match_data` if applicable.
    ///
    /// This uses reverse matching.
    fn ante_matches(
        &self,
        matcher: &mut impl Utf8Matcher<Reverse>,
        match_data: &mut MatchData,
        vt: &VarTable,
    ) -> bool {
        if self.ante.is_empty() {
            // fast path for empty queries, which always match
            return true;
        }
        rev_match_str_with_specials(&self.ante, matcher, match_data, vt)
    }

    /// Returns whether the post context matches or not. Fills in `match_data` if applicable.
    ///
    /// This uses forward matching.
    fn post_matches(
        &self,
        matcher: &mut impl Utf8Matcher<Forward>,
        match_data: &mut MatchData,
        vt: &VarTable,
    ) -> bool {
        if self.post.is_empty() {
            // fast path for empty queries, which always match
            return true;
        }
        match_str_with_specials(&self.post, matcher, match_data, vt)
    }

    /// Returns whether the post context matches or not. Fills in `match_data` if applicable.
    ///
    /// This uses forward matching.
    fn key_matches(
        &self,
        matcher: &mut impl Utf8Matcher<Forward>,
        match_data: &mut MatchData,
        vt: &VarTable,
    ) -> bool {
        if self.key.is_empty() {
            // fast path for empty queries, which always match
            return true;
        }
        match_str_with_specials(&self.key, matcher, match_data, vt)
    }
}

/// Returns the index of the first special construct that is encoded as a private use char in `s`,
/// if there is one. Returns `None` if the passed string is pure
/// (contains no encoded special constructs).
fn find_special(s: &str) -> Option<usize> {
    // note: full-match (i.e., if this function returns Some(_) or None, could be
    // precomputed + stored at datagen time (there could eg be a reserved char that is at the
    // start/end of key <=> key is completely pure)
    s.char_indices()
        .find(|(_, c)| VarTable::ENCODE_RANGE.contains(c))
        .map(|(i, _)| i)
}

/// Returns the index of the char to the right of the first (from the right) special construct
/// encoded as a private use char. Returns `None` if the passed string is pure
/// (contains no encoded special constructs).
fn rev_find_special(s: &str) -> Option<usize> {
    s.char_indices()
        .rfind(|(_, c)| VarTable::ENCODE_RANGE.contains(c))
        .map(|(i, c)| i + c.len_utf8())
}

/// Recursively estimates the size of the replacement string.
fn estimate_replacement_size(replacement: &str, data: &MatchData, vt: &VarTable) -> usize {
    let mut size;
    let replacement_tail;

    match find_special(replacement) {
        None => return replacement.len(),
        Some(idx) => {
            size = idx;
            replacement_tail = &replacement[idx..];
        }
    }

    for rep_c in replacement_tail.chars() {
        if !VarTable::ENCODE_RANGE.contains(&rep_c) {
            // regular char
            size += rep_c.len_utf8();
            continue;
        }
        // must be special replacer

        let replacer = match vt.lookup_replacer(rep_c) {
            Some(replacer) => replacer,
            None => {
                debug_assert!(false, "invalid encoded special {rep_c:?}");
                // GIGO behavior. we just skip invalid encodings
                continue;
            }
        };
        size += replacer.estimate_size(data, vt);
    }

    size
}

/// Applies the replacements from the `replacement`, which may contain encoded special replacers, to `dest`,
/// including non-default cursor updates.
fn replace_str_with_specials(
    replacement: &str,
    dest: &mut Insertable,
    data: &MatchData,
    vt: &VarTable,
    env: &Env,
) {
    let replacement = match find_special(replacement) {
        None => {
            // pure string
            dest.push_str(replacement);
            return;
        }
        Some(idx) => {
            dest.push_str(&replacement[..idx]);
            &replacement[idx..]
        }
    };

    for rep_c in replacement.chars() {
        if !VarTable::ENCODE_RANGE.contains(&rep_c) {
            // regular char
            dest.push(rep_c);
            continue;
        }
        // must be special replacer

        let replacer = match vt.lookup_replacer(rep_c) {
            Some(replacer) => replacer,
            None => {
                debug_assert!(false, "invalid encoded special {rep_c:?}");
                // GIGO behavior. we just skip invalid encodings
                continue;
            }
        };
        replacer.replace(dest, data, vt, env);
    }
}

/// Tries to match `query`, which may contain encoded special matchers, on `matcher`. Fills in `match_data` if applicable.
fn match_str_with_specials(
    query: &str,
    matcher: &mut impl Utf8Matcher<Forward>,
    match_data: &mut MatchData,
    vt: &VarTable,
) -> bool {
    // eprintln!("trying to match query {query:?} on input {matcher:?}");

    let query = match find_special(query) {
        None => {
            // pure string
            return matcher.match_and_consume_str(query);
        }
        Some(idx) => {
            if !matcher.match_and_consume_str(&query[..idx]) {
                return false;
            }
            &query[idx..]
        }
    };

    // iterate char-by-char, and try to match each char
    // note: might be good to avoid the UTF-8 => char conversion?
    for query_c in query.chars() {
        if !VarTable::ENCODE_RANGE.contains(&query_c) {
            // regular char
            if !matcher.match_and_consume_char(query_c) {
                return false;
            }
            continue;
        }
        // must be special matcher

        let special_matcher = match vt.lookup_matcher(query_c) {
            Some(matcher) => matcher,
            None => {
                debug_assert!(false, "invalid encoded special {query_c:?}");
                // GIGO behavior. we just skip invalid encodings
                continue;
            }
        };
        if !special_matcher.matches(matcher, match_data, vt) {
            return false;
        }
    }

    // matched the full query string successfully
    true
}

/// Tries to match `query`, which may contain encoded special matchers, on `matcher` from the right. Fills in `match_data` if applicable.
fn rev_match_str_with_specials(
    query: &str,
    matcher: &mut impl Utf8Matcher<Reverse>,
    match_data: &mut MatchData,
    vt: &VarTable,
) -> bool {
    let query = match rev_find_special(query) {
        None => {
            // pure string
            return matcher.match_and_consume_str(query);
        }
        Some(idx) => {
            if !matcher.match_and_consume_str(&query[idx..]) {
                return false;
            }
            &query[..idx]
        }
    };

    // iterate char-by-char, and try to match each char
    // note: might be good to avoid the UTF-8 => char conversion?
    for query_c in query.chars().rev() {
        if !VarTable::ENCODE_RANGE.contains(&query_c) {
            // regular char
            if !matcher.match_and_consume_char(query_c) {
                return false;
            }
            continue;
        }
        // must be special matcher

        let special_matcher = match vt.lookup_matcher(query_c) {
            Some(matcher) => matcher,
            None => {
                debug_assert!(false, "invalid encoded special {query_c:?}");
                // GIGO behavior. we just skip invalid encodings
                continue;
            }
        };
        if !special_matcher.rev_matches(matcher, match_data, vt) {
            return false;
        }
    }

    // matched the full query string successfully
    true
}

/// Stores the state for a single conversion rule.
#[derive(Debug)]
struct MatchData {
    /// Stored matches of segments.
    segments: Vec<String>,
}

impl MatchData {
    fn new() -> Self {
        Self {
            segments: Vec::new(),
        }
    }

    fn update_segment(&mut self, i: usize, s: String) {
        if i >= self.segments.len() {
            self.segments.resize_with(i + 1, Default::default);
        }
        self.segments[i] = s;
    }

    fn get_segment(&self, i: usize) -> &str {
        if let Some(s) = self.segments.get(i) {
            return s;
        }
        // two cases: we have not (at runtime) encountered a segment with a high enough index
        // to populate this index, in which case it is defined to be "",
        // or this is GIGO, which is again ""
        ""
    }
}

enum QuantifierKind {
    ZeroOrOne,
    ZeroOrMore,
    OneOrMore,
}

enum SpecialMatcher<'a> {
    Compound(&'a str),
    Quantifier(QuantifierKind, &'a str),
    Segment(Segment<'a>),
    UnicodeSet(CodePointInversionListAndStringList<'a>),
    AnchorStart,
    AnchorEnd,
}

impl SpecialMatcher<'_> {
    // Thought: a lot of duplicated code in matches and rev_matches. deduplicate.
    //  maybe by being generic over Direction? doesn't work for some special cases, though, like segments and sets

    /// Returns whether there is a match or not. Fills in `match_data` if applicable.
    fn matches(
        &self,
        matcher: &mut impl Utf8Matcher<Forward>,
        match_data: &mut MatchData,
        vt: &VarTable,
    ) -> bool {
        match self {
            Self::Compound(query) => match_str_with_specials(query, matcher, match_data, vt),
            Self::UnicodeSet(set) => {
                // eprintln!("checking if set {set:?} matches input {matcher:?}");

                if matcher.is_empty() {
                    if set.contains_str("") {
                        return true;
                    }
                    if set.contains_str("\u{FFFF}") {
                        if matcher.match_end_anchor() {
                            return true;
                        }
                        if matcher.match_start_anchor() {
                            return true;
                        }
                    }
                    // only an empty string or an anchor could match an empty input
                    return false;
                }

                let mut max_str_match: Option<usize> = None;
                for s in set.strings().iter() {
                    // strings are sorted. we can optimize by early-breaking when we encounter
                    // an `s` that is lexicographically larger than `input`

                    if matcher.match_str(s) {
                        max_str_match = max_str_match.map(|m| m.max(s.len())).or(Some(s.len()));
                        continue;
                    }

                    match (s.chars().next(), matcher.next_char()) {
                        // break early. since s_c is > input_c, we know that s > input, thus all
                        // strings from here on out are > input, and thus cannot match
                        (Some(s_c), Some(input_c)) if s_c > input_c => break,
                        _ => (),
                    }
                }
                if let Some(max) = max_str_match {
                    // some string matched
                    return matcher.consume(max);
                }

                if let Some(input_c) = matcher.next_char() {
                    // eprintln!("checking if set {set:?} contains char {input_c:?}");
                    if set.contains(input_c) {
                        // eprintln!("contains!");
                        return matcher.consume(input_c.len_utf8());
                    }
                }

                false
            }
            Self::AnchorEnd => matcher.match_end_anchor(),
            Self::AnchorStart => matcher.match_start_anchor(),
            Self::Segment(segment) => {
                // Thought: Would it be cool to have a similar functionality as Insertable::start_replaceable_adapter
                //  that takes care of this `start`/`end` shenanigans? here it's not a safety issue, merely a panic issue.
                let start = matcher.cursor();
                if !match_str_with_specials(&segment.content, matcher, match_data, vt) {
                    return false;
                }
                let end = matcher.cursor();
                let matched = matcher.str_range(start..end).unwrap();
                // note: at the moment we could just store start..end
                match_data.update_segment(segment.idx as usize, matched.to_string());
                true
            }
            Self::Quantifier(kind, query) => {
                let (min_matches, max_matches) = match kind {
                    QuantifierKind::ZeroOrOne => (0, 1),
                    QuantifierKind::ZeroOrMore => (0, usize::MAX),
                    QuantifierKind::OneOrMore => (1, usize::MAX),
                };

                let mut matches = 0;

                while matches < max_matches {
                    let pre_cursor = matcher.cursor();
                    if !match_str_with_specials(query, matcher, match_data, vt) {
                        break;
                    }
                    let post_cursor = matcher.cursor();
                    matches += 1;
                    if pre_cursor == post_cursor {
                        // no progress was made but there was still a match. this means we could
                        // recurse infinitely. break out of the loop.
                        break;
                    }
                }

                matches >= min_matches
            }
        }
    }

    /// Returns whether there is a match or not. Fills in `match_data` if applicable.
    fn rev_matches(
        &self,
        matcher: &mut impl Utf8Matcher<Reverse>,
        match_data: &mut MatchData,
        vt: &VarTable,
    ) -> bool {
        match self {
            Self::Compound(query) => rev_match_str_with_specials(query, matcher, match_data, vt),
            Self::UnicodeSet(set) => {
                // eprintln!("checking if set {set:?} reverse matches input {matcher:?}");

                if matcher.is_empty() {
                    if set.contains_str("") {
                        return true;
                    }
                    if set.contains_str("\u{FFFF}") {
                        if matcher.match_end_anchor() {
                            return true;
                        }
                        if matcher.match_start_anchor() {
                            return true;
                        }
                    }
                    // only an empty string or an anchor could match an empty input
                    return false;
                }

                // because we are reverse matching, we cannot do the same early breaking as in
                // forward matching.
                let max_str_match = set
                    .strings()
                    .iter()
                    .filter(|s| matcher.match_str(s))
                    .map(str::len)
                    .max();
                if let Some(max) = max_str_match {
                    // some string matched
                    return matcher.consume(max);
                }

                if let Some(input_c) = matcher.next_char() {
                    // eprintln!("checking if set {set:?} contains char {input_c:?}");
                    if set.contains(input_c) {
                        // eprintln!("contains!");
                        return matcher.consume(input_c.len_utf8());
                    }
                }

                false
            }
            Self::AnchorEnd => matcher.match_end_anchor(),
            Self::AnchorStart => matcher.match_start_anchor(),
            Self::Segment(segment) => {
                let end = matcher.cursor();
                if !rev_match_str_with_specials(&segment.content, matcher, match_data, vt) {
                    return false;
                }
                let start = matcher.cursor();
                let matched = &matcher.str_range(start..end).unwrap();
                // note: at the moment we could just store start..end
                match_data.update_segment(segment.idx as usize, matched.to_string());
                true
            }
            Self::Quantifier(kind, query) => {
                let (min_matches, max_matches) = match kind {
                    QuantifierKind::ZeroOrOne => (0, 1),
                    QuantifierKind::ZeroOrMore => (0, usize::MAX),
                    QuantifierKind::OneOrMore => (1, usize::MAX),
                };

                let mut matches = 0;

                while matches < max_matches {
                    let pre_cursor = matcher.cursor();
                    if !rev_match_str_with_specials(query, matcher, match_data, vt) {
                        break;
                    }
                    let post_cursor = matcher.cursor();
                    matches += 1;
                    if pre_cursor == post_cursor {
                        // no progress was made but there was still a match. this means we could
                        // recurse infinitely. break out of the loop.
                        break;
                    }
                }

                matches >= min_matches
            }
        }
    }
}

enum SpecialReplacer<'a> {
    Compound(&'a str),
    FunctionCall(FunctionCall<'a>),
    BackReference(u16),
    LeftPlaceholderCursor(u16),
    RightPlaceholderCursor(u16),
    PureCursor,
}

impl SpecialReplacer<'_> {
    /// Estimates the size of the replacement string produced by this Replacer.
    fn estimate_size(&self, data: &MatchData, vt: &VarTable) -> usize {
        match self {
            Self::Compound(replacer) => estimate_replacement_size(replacer, data, vt),
            Self::FunctionCall(call) => {
                // this is the only inexact case, so we estimate that the transliteration will stay
                // roughly the same size as the input
                estimate_replacement_size(&call.arg, data, vt)
            }
            &Self::BackReference(num) => data.get_segment(num as usize).len(),
            Self::LeftPlaceholderCursor(_) | Self::RightPlaceholderCursor(_) | Self::PureCursor => {
                0
            }
        }
    }

    /// Applies the replacement from this replacer to `dest`. Also applies any updates to the cursor.
    fn replace(&self, dest: &mut Insertable, data: &MatchData, vt: &VarTable, env: &Env) {
        match self {
            Self::Compound(replacer) => replace_str_with_specials(replacer, dest, data, vt, env),
            Self::PureCursor => dest.set_offset_to_here(),
            &Self::LeftPlaceholderCursor(num) => {
                // must occur at the very end of a replacement
                dest.set_offset_to_chars_off_end(num);
            }
            &Self::RightPlaceholderCursor(num) => {
                // must occur at the very beginning of the replacement
                debug_assert_eq!(
                    dest.curr_replacement_len(),
                    0,
                    "pre-start cursor not the first replacement"
                );
                dest.set_offset_to_chars_off_start(num);
            }
            &Self::BackReference(num) => {
                dest.push_str(data.get_segment(num as usize));
            }
            Self::FunctionCall(call) => {
                // the way function call replacing works is as such:
                // use the same backing buffer as the parent Replaceable, but set
                // the visible content range of the Replaceable appropriately.

                // eprintln!("dest before function call: {dest:?}");

                let mut range_aggregator = dest.start_replaceable_adapter();
                replace_str_with_specials(&call.arg, &mut range_aggregator, data, vt, env);

                call.translit
                    .transliterate(range_aggregator.as_replaceable().child(), env);
            }
        }
    }
}

enum VarTableElement<'a> {
    Compound(&'a str),
    Quantifier(QuantifierKind, &'a str),
    Segment(Segment<'a>),
    UnicodeSet(CodePointInversionListAndStringList<'a>),
    FunctionCall(FunctionCall<'a>),
    BackReference(u16),
    AnchorStart,
    AnchorEnd,
    LeftPlaceholderCursor(u16),
    RightPlaceholderCursor(u16),
    PureCursor,
}

impl<'a> VarTableElement<'a> {
    fn into_replacer(self) -> Option<SpecialReplacer<'a>> {
        Some(match self {
            Self::Compound(elt) => SpecialReplacer::Compound(elt),
            Self::FunctionCall(elt) => SpecialReplacer::FunctionCall(elt),
            Self::BackReference(elt) => SpecialReplacer::BackReference(elt),
            Self::LeftPlaceholderCursor(elt) => SpecialReplacer::LeftPlaceholderCursor(elt),
            Self::RightPlaceholderCursor(elt) => SpecialReplacer::RightPlaceholderCursor(elt),
            Self::PureCursor => SpecialReplacer::PureCursor,
            _ => return None,
        })
    }

    fn into_matcher(self) -> Option<SpecialMatcher<'a>> {
        Some(match self {
            Self::Compound(elt) => SpecialMatcher::Compound(elt),
            Self::Quantifier(kind, elt) => SpecialMatcher::Quantifier(kind, elt),
            Self::Segment(elt) => SpecialMatcher::Segment(elt),
            Self::UnicodeSet(elt) => SpecialMatcher::UnicodeSet(elt),
            Self::AnchorEnd => SpecialMatcher::AnchorEnd,
            Self::AnchorStart => SpecialMatcher::AnchorStart,
            _ => return None,
        })
    }
}

impl<'a> VarTable<'a> {
    fn lookup(&'a self, query: char) -> Option<VarTableElement<'a>> {
        match query {
            Self::BASE..=Self::MAX_DYNAMIC => {}
            Self::RESERVED_PURE_CURSOR => return Some(VarTableElement::PureCursor),
            Self::RESERVED_ANCHOR_END => return Some(VarTableElement::AnchorEnd),
            Self::RESERVED_ANCHOR_START => return Some(VarTableElement::AnchorStart),
            _ => return None,
        };
        let idx = query as u32 - Self::BASE as u32;
        let mut idx = idx as usize;

        // TODO(#3957): might it be worth trying to speed up these lookups by binary searching?
        // TODO(#3957): we can special-case lookup_matcher, lookup_replacer. lookup_matcher does not need
        //  to check past UnicodeSets, lookup_replacer needs to check the range for Compounds, then skip
        //  quantifiers, segments, unicodesets completely, then continue with segments, function calls,
        //  cursors and backreferences
        let mut next_base = self.compounds.len();
        if idx < next_base {
            return Some(VarTableElement::Compound(&self.compounds[idx]));
        }
        // no underflow for all these idx subtractions, as idx is always >= next_base
        idx -= next_base;
        next_base = self.quantifiers_opt.len();
        if idx < next_base {
            return Some(VarTableElement::Quantifier(
                QuantifierKind::ZeroOrOne,
                &self.quantifiers_opt[idx],
            ));
        }
        idx -= next_base;
        next_base = self.quantifiers_kleene.len();
        if idx < next_base {
            return Some(VarTableElement::Quantifier(
                QuantifierKind::ZeroOrMore,
                &self.quantifiers_kleene[idx],
            ));
        }
        idx -= next_base;
        next_base = self.quantifiers_kleene_plus.len();
        if idx < next_base {
            return Some(VarTableElement::Quantifier(
                QuantifierKind::OneOrMore,
                &self.quantifiers_kleene_plus[idx],
            ));
        }
        idx -= next_base;
        next_base = self.segments.len();
        if idx < next_base {
            return Some(VarTableElement::Segment(Segment::zero_from(
                &self.segments[idx],
            )));
        }
        idx -= next_base;
        next_base = self.unicode_sets.len();
        if idx < next_base {
            return Some(VarTableElement::UnicodeSet(
                CodePointInversionListAndStringList::zero_from(&self.unicode_sets[idx]),
            ));
        }
        idx -= next_base;
        next_base = self.function_calls.len();
        if idx < next_base {
            return Some(VarTableElement::FunctionCall(FunctionCall::zero_from(
                &self.function_calls[idx],
            )));
        }
        idx -= next_base;
        next_base = self.max_left_placeholder_count as usize;
        if idx < next_base {
            // + 1 because index 0 represents 1 placeholder, etc.
            // cast is guaranteed because query is inside a range of less than 2^16 elements
            return Some(VarTableElement::LeftPlaceholderCursor(idx as u16 + 1));
        }
        idx -= next_base;
        next_base = self.max_right_placeholder_count as usize;
        if idx < next_base {
            // + 1 because index 0 represents 1 placeholder, etc.
            // cast is guaranteed because query is inside a range of less than 2^16 elements
            return Some(VarTableElement::RightPlaceholderCursor(idx as u16 + 1));
        }
        idx -= next_base;
        // idx must be a backreference (an u16 encoded as <itself> indices past the last valid other index)
        // cast is guaranteed because query is inside a range of less than 2^16 elements
        Some(VarTableElement::BackReference(idx as u16))
    }

    fn lookup_matcher(&'a self, query: char) -> Option<SpecialMatcher<'a>> {
        let elt = self.lookup(query)?;
        elt.into_matcher()
    }

    fn lookup_replacer(&'a self, query: char) -> Option<SpecialReplacer<'a>> {
        let elt = self.lookup(query)?;
        elt.into_replacer()
    }
}

#[cfg(test)]
mod tests {
    #![allow(unused_qualifications)]
    use super::*;
    use crate::transliterate::RuleCollection;

    #[test]
    fn test_empty_matches() {
        let cases = [
            ("ax", "amatch"),
            ("a", "a"),
            ("a1", "amatch1"),
            ("b", "b"),
            ("b1", "bmatch1"),
        ];

        let mut collection = RuleCollection::default();
        collection.register_source(
            &"und-x-test".parse().unwrap(),
            include_str!("../../../tests/transliterate/data/transforms/EmptyMatches.txt").into(),
            [],
            false,
            true,
        );

        let t = Transliterator::try_new_unstable(
            &collection.as_provider(),
            &icu_normalizer::provider::Baked,
            &icu_casemap::provider::Baked,
            &"und-x-test".parse().unwrap(),
        )
        .unwrap();

        for (input, output) in cases {
            assert_eq!(t.transliterate(input.to_string()), output);
        }
    }

    #[test]
    fn test_recursive_suite() {
        let mut collection = RuleCollection::default();
        collection.register_source(
            &"und-x-root".parse().unwrap(),
            include_str!("../../../tests/transliterate/data/transforms/RecursiveRoot.txt").into(),
            [],
            false,
            true,
        );
        collection.register_source(
            &"und-x-rec".parse().unwrap(),
            include_str!("../../../tests/transliterate/data/transforms/RecursiveA.txt").into(),
            ["Test-Test/RecursiveSuiteA"],
            false,
            true,
        );

        let t = Transliterator::try_new_unstable(
            &collection.as_provider(),
            &icu_normalizer::provider::Baked,
            &icu_casemap::provider::Baked,
            &"und-x-root".parse().unwrap(),
        )
        .unwrap();

        let input = "XXXabcXXXdXXe";
        let output = "XXXXXXaWORKEDcXXXXXXdXXXXXe";
        assert_eq!(t.transliterate(input.to_string()), output);
    }

    #[test]
    fn test_cursor_placeholders_filters() {
        let mut collection = RuleCollection::default();
        collection.register_source(
            &"und-x-test".parse().unwrap(),
            include_str!("../../../tests/transliterate/data/transforms/CursorFilters.txt").into(),
            [],
            false,
            true,
        );
        let t = Transliterator::try_new_unstable(
            &collection.as_provider(),
            &icu_normalizer::provider::Baked,
            &icu_casemap::provider::Baked,
            &"und-x-test".parse().unwrap(),
        )
        .unwrap();

        let input = "xa";
        let output = "xb";
        assert_eq!(t.transliterate(input.to_string()), output);
    }

    #[test]
    fn test_functionality() {
        let mut collection = RuleCollection::default();
        collection.register_source(
            &"und-x-test".parse().unwrap(),
            include_str!("../../../tests/transliterate/data/transforms/Functionality.txt").into(),
            [],
            false,
            true,
        );
        let t = Transliterator::try_new_unstable(
            &collection.as_provider(),
            &icu_normalizer::provider::Baked,
            &icu_casemap::provider::Baked,
            &"und-x-test".parse().unwrap(),
        )
        .unwrap();

        let input = "abädefghijkl!";
        let output = "FIfiunremovedtbxyzftbxyzxyzXYZjkT!";
        assert_eq!(t.transliterate(input.to_string()), output);
    }

    #[test]
    fn test_de_ascii() {
        let t = Transliterator::try_new(&"de-t-de-d0-ascii".parse().unwrap()).unwrap();
        let input =
            "Über ältere Lügner lästern ist sehr a\u{0308}rgerlich. Ja, SEHR ÄRGERLICH! - ꜵ";
        let output =
            "Ueber aeltere Luegner laestern ist sehr aergerlich. Ja, SEHR AERGERLICH! - ao";
        assert_eq!(t.transliterate(input.to_string()), output);
    }

    #[test]
    fn test_override() {
        #[derive(Debug)]
        struct MaoamTranslit;
        impl CustomTransliterator for MaoamTranslit {
            fn transliterate(&self, input: &str, range: Range<usize>) -> String {
                let input = &input[range];
                input.replace('ꜵ', "maoam")
            }
        }

        let want_locale = "und-t-und-latn-d0-ascii".parse().unwrap();
        let t =
            Transliterator::try_new_with_override(&"de-t-de-d0-ascii".parse().unwrap(), |locale| {
                locale
                    .eq(&want_locale)
                    .then_some(Ok(Box::new(MaoamTranslit)))
            })
            .unwrap();

        let input = "Ich liebe ꜵ über alles";
        let output = "Ich liebe maoam ueber alles";
        assert_eq!(t.transliterate(input.to_string()), output);
    }

    #[test]
    fn test_nfc_nfd() {
        let t = Transliterator::try_new(&"und-t-und-latn-d0-ascii".parse().unwrap()).unwrap();
        let input = "äa\u{0308}";
        let output = "aa";
        assert_eq!(t.transliterate(input.to_string()), output);
    }

    #[test]
    fn test_hex_rust() {
        let mut collection = RuleCollection::default();
        collection.register_source(
            &"und-x-test".parse().unwrap(),
            "::Hex/Rust;".into(),
            [],
            false,
            true,
        );
        let t = Transliterator::try_new_unstable(
            &collection.as_provider(),
            &icu_normalizer::provider::Baked,
            &icu_casemap::provider::Baked,
            &"und-x-test".parse().unwrap(),
        )
        .unwrap();
        let input = "\0äa\u{10FFFF}❤!";
        let output = r"\u{00}\u{e4}\u{61}\u{10ffff}\u{2764}\u{21}";
        assert_eq!(t.transliterate(input.to_string()), output);
    }

    #[test]
    fn test_hex_unicode() {
        let mut collection = RuleCollection::default();
        collection.register_source(
            &"und-x-test".parse().unwrap(),
            "::Hex/Unicode;".into(),
            [],
            false,
            true,
        );
        let t = Transliterator::try_new_unstable(
            &collection.as_provider(),
            &icu_normalizer::provider::Baked,
            &icu_casemap::provider::Baked,
            &"und-x-test".parse().unwrap(),
        )
        .unwrap();
        let input = "\0äa\u{10FFFF}❤!";
        let output = "U+0000U+00E4U+0061U+10FFFFU+2764U+0021";
        assert_eq!(t.transliterate(input.to_string()), output);
    }

    #[test]
    fn test_katakana_hiragana() {
        let t = Transliterator::try_new(&"und-Hira-t-und-kana".parse().unwrap()).unwrap();
        let input = "ウィキペディアへようこそ";
        let output = "うぃきぺでぃあへようこそ";
        assert_eq!(t.transliterate(input.to_string()), output);
    }
}
