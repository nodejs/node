// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains most of the actual algorithms for case mapping.
//!
//! Primarily, it implements methods on `CaseMap`, which contains the data model.

use crate::greek_to_me::{
    self, GreekCombiningCharacterSequenceDiacritics, GreekDiacritics, GreekPrecomposedLetterData,
    GreekVowel,
};
use crate::provider::data::{DotType, MappingKind};
use crate::provider::exception_helpers::ExceptionSlot;
use crate::provider::{CaseMap, CaseMapUnfold};
use crate::set::ClosureSink;
use crate::titlecase::TrailingCase;
use core::fmt;
use icu_locale_core::LanguageIdentifier;
use writeable::Writeable;

const ACUTE: char = '\u{301}';

// Used to control the behavior of CaseMapper::fold.
// Currently only used to decide whether to use Turkic (T) mappings for dotted/dotless i.
#[derive(Copy, Clone, Default)]
pub(crate) struct FoldOptions {
    exclude_special_i: bool,
}

impl FoldOptions {
    pub fn with_turkic_mappings() -> Self {
        Self {
            exclude_special_i: true,
        }
    }
}

/// Helper type that wraps a writeable in a prefix string
pub(crate) struct StringAndWriteable<'a, W> {
    pub string: &'a str,
    pub writeable: W,
}

impl<Wr: Writeable> Writeable for StringAndWriteable<'_, Wr> {
    fn write_to<W: fmt::Write + ?Sized>(&self, sink: &mut W) -> fmt::Result {
        sink.write_str(self.string)?;
        self.writeable.write_to(sink)
    }
    fn writeable_length_hint(&self) -> writeable::LengthHint {
        writeable::LengthHint::exact(self.string.len()) + self.writeable.writeable_length_hint()
    }
}

pub(crate) struct FullCaseWriteable<'a, 'data, const IS_TITLE_CONTEXT: bool> {
    data: &'data CaseMap<'data>,
    src: &'a str,
    locale: CaseMapLocale,
    mapping: MappingKind,
    titlecase_tail_casing: TrailingCase,
}

impl<'a, const IS_TITLE_CONTEXT: bool> Writeable for FullCaseWriteable<'a, '_, IS_TITLE_CONTEXT> {
    fn write_to<W: fmt::Write + ?Sized>(&self, sink: &mut W) -> fmt::Result {
        let src = self.src;
        let mut mapping = self.mapping;
        let mut iter = src.char_indices();
        for (i, c) in &mut iter {
            let context = ContextIterator::new(&src[..i], &src[i..]);
            self.data
                .full_helper::<IS_TITLE_CONTEXT, W>(c, context, self.locale, mapping, sink)?;
            if IS_TITLE_CONTEXT {
                if self.titlecase_tail_casing == TrailingCase::Lower {
                    mapping = MappingKind::Lower;
                } else {
                    break;
                }
            }
        }
        // Write the rest of the string
        if IS_TITLE_CONTEXT && self.titlecase_tail_casing == TrailingCase::Unchanged {
            sink.write_str(iter.as_str())?;
        }
        Ok(())
    }
    fn writeable_length_hint(&self) -> writeable::LengthHint {
        writeable::LengthHint::at_least(self.src.len())
    }
    fn write_to_string(&self) -> alloc::borrow::Cow<'a, str> {
        writeable::to_string_or_borrow(self, self.src.as_bytes())
    }
}

impl<'data> CaseMap<'data> {
    fn simple_helper(&self, c: char, kind: MappingKind) -> char {
        let data = self.lookup_data(c);
        if !data.has_exception() {
            if data.is_relevant_to(kind) {
                let folded = c as i32 + data.delta() as i32;
                // GIGO: delta should be valid
                char::from_u32(folded as u32).unwrap_or(c)
            } else {
                c
            }
        } else {
            let idx = data.exception_index();
            let exception = self.exceptions.get(idx);
            if data.is_relevant_to(kind) {
                if let Some(simple) = exception.get_simple_case_slot_for(c) {
                    return simple;
                }
            }
            exception.slot_char_for_kind(kind).unwrap_or(c)
        }
    }

    // Returns the lowercase mapping of the given `char`.
    #[inline]
    pub(crate) fn simple_lower(&self, c: char) -> char {
        self.simple_helper(c, MappingKind::Lower)
    }

    // Returns the uppercase mapping of the given `char`.
    #[inline]
    pub(crate) fn simple_upper(&self, c: char) -> char {
        self.simple_helper(c, MappingKind::Upper)
    }

    // Returns the titlecase mapping of the given `char`.
    #[inline]
    pub(crate) fn simple_title(&self, c: char) -> char {
        self.simple_helper(c, MappingKind::Title)
    }

    // Return the simple case folding mapping of the given char.
    #[inline]
    pub(crate) fn simple_fold(&self, c: char, options: FoldOptions) -> char {
        let data = self.lookup_data(c);
        if !data.has_exception() {
            if data.is_upper_or_title() {
                let folded = c as i32 + data.delta() as i32;
                // GIGO: delta should be valid
                char::from_u32(folded as u32).unwrap_or(c)
            } else {
                c
            }
        } else {
            // TODO: if we move conditional fold and no_simple_case_folding into
            // simple_helper, this function can just call simple_helper.
            let idx = data.exception_index();
            let exception = self.exceptions.get(idx);
            if exception.bits.has_conditional_fold() {
                self.simple_fold_special_case(c, options)
            } else if exception.bits.no_simple_case_folding() {
                c
            } else if data.is_upper_or_title() && exception.has_slot(ExceptionSlot::Delta) {
                // unwrap_or case should never happen but best to avoid panics
                exception.get_simple_case_slot_for(c).unwrap_or('\0')
            } else if let Some(slot_char) = exception.slot_char_for_kind(MappingKind::Fold) {
                slot_char
            } else {
                c
            }
        }
    }

    fn dot_type(&self, c: char) -> DotType {
        let data = self.lookup_data(c);
        if !data.has_exception() {
            data.dot_type()
        } else {
            let idx = data.exception_index();
            self.exceptions.get(idx).bits.dot_type()
        }
    }

    // Returns true if this code point is is case-sensitive.
    // This is not currently exposed.
    #[allow(dead_code)]
    fn is_case_sensitive(&self, c: char) -> bool {
        let data = self.lookup_data(c);
        if !data.has_exception() {
            data.is_sensitive()
        } else {
            let idx = data.exception_index();
            self.exceptions.get(idx).bits.is_sensitive()
        }
    }

    /// Returns whether the character is cased
    pub(crate) fn is_cased(&self, c: char) -> bool {
        self.lookup_data(c).case_type().is_some()
    }

    #[inline(always)]
    // IS_TITLE_CONTEXT must be true if kind is MappingKind::Title
    // The kind may be a different kind with IS_TITLE_CONTEXT still true because
    // titlecasing a segment involves switching to lowercase later
    fn full_helper<const IS_TITLE_CONTEXT: bool, W: fmt::Write + ?Sized>(
        &self,
        c: char,
        context: ContextIterator,
        locale: CaseMapLocale,
        kind: MappingKind,
        sink: &mut W,
    ) -> fmt::Result {
        // If using a title mapping IS_TITLE_CONTEXT must be true
        debug_assert!(kind != MappingKind::Title || IS_TITLE_CONTEXT);
        // In a title context, kind MUST be Title or Lower
        debug_assert!(
            !IS_TITLE_CONTEXT || kind == MappingKind::Title || kind == MappingKind::Lower
        );

        // ICU4C's non-standard extension for Dutch IJ titlecasing
        // handled here instead of in full_lower_special_case because J does not have conditional
        // special casemapping.
        if IS_TITLE_CONTEXT && locale == CaseMapLocale::Dutch && kind == MappingKind::Lower {
            // When titlecasing, a J found immediately after an I at the beginning of the segment
            // should also uppercase. They are both allowed to have an acute accent but it must
            // be present on both letters or neither. They may not have any other combining marks.
            if (c == 'j' || c == 'J') && context.is_dutch_ij_pair_at_beginning(self) {
                return sink.write_char('J');
            }
        }

        // ICU4C's non-standard extension for Greek uppercasing:
        // https://icu.unicode.org/design/case/greek-upper.
        // Effectively removes Greek accents from Greek vowels during uppercasing,
        // whilst attempting to preserve additional marks like the dialytika (diæresis)
        // and ypogegrammeni (combining small iota).
        if !IS_TITLE_CONTEXT && locale == CaseMapLocale::Greek && kind == MappingKind::Upper {
            // Remove all combining diacritics on a Greek letter.
            // Ypogegrammeni is not an accent mark and is handled by regular casemapping (it turns into
            // a capital iota).
            // The dialytika is removed here, but it may be added again when the base letter is being processed.
            if greek_to_me::is_greek_diacritic_except_ypogegrammeni(c)
                && context.preceded_by_greek_letter()
            {
                return Ok(());
            }
            let data = greek_to_me::get_data(c);
            // Check if the character is a Greek vowel
            match data {
                Some(GreekPrecomposedLetterData::Vowel(vowel, mut precomposed_diacritics)) => {
                    // Get the diacritics on the character itself, and add any further combining diacritics
                    // from the context.
                    let mut diacritics = context.add_greek_diacritics(precomposed_diacritics);
                    // If the previous vowel had an accent (which would be removed) but no dialytika,
                    // and this is an iota or upsilon, add a dialytika since it is necessary to disambiguate
                    // the now-unaccented adjacent vowels from a digraph/diphthong.
                    // Use a precomposed dialytika if the accent was precomposed, and a combining dialytika
                    // if the accent was combining, so as to map NFD to NFD and NFC to NFC.
                    if !diacritics.dialytika && (vowel == GreekVowel::Ι || vowel == GreekVowel::Υ)
                    {
                        if let Some(preceding_vowel) = context.preceding_greek_vowel_diacritics() {
                            if !preceding_vowel.combining.dialytika
                                && !preceding_vowel.precomposed.dialytika
                            {
                                if preceding_vowel.combining.accented {
                                    diacritics.dialytika = true;
                                } else {
                                    precomposed_diacritics.dialytika =
                                        preceding_vowel.precomposed.accented;
                                }
                            }
                        }
                    }
                    // Write the base of the uppercased combining character sequence.
                    // In most branches this is [`upper_base`], i.e., the uppercase letter with all accents removed.
                    // In some branches the base has a precomposed diacritic.
                    // In the case of the Greek disjunctive "or", a combining tonos may also be written.
                    match vowel {
                        GreekVowel::Η => {
                            // The letter η (eta) is allowed to retain a tonos when it is form a single-letter word to distinguish
                            // the feminine definite article ἡ (monotonic η) from the disjunctive "or" ἤ (monotonic ή).
                            //
                            // A lone η with an accent other than the oxia/tonos is not expected,
                            // so there is no need to special-case the oxia/tonos.
                            // The ancient ᾖ (exist.PRS.SUBJ.3s) has a iota subscript as well as the circumflex,
                            // so it would not be given an oxia/tonos under this rule, and the subjunctive is formed with a particle
                            // (e.g. να είναι) since Byzantine times anyway.
                            if diacritics.accented
                                && !context.followed_by_cased_letter(self)
                                && !context.preceded_by_cased_letter(self)
                                && !diacritics.ypogegrammeni
                            {
                                if precomposed_diacritics.accented {
                                    sink.write_char('Ή')?;
                                } else {
                                    sink.write_char('Η')?;
                                    sink.write_char(greek_to_me::TONOS)?;
                                }
                            } else {
                                sink.write_char('Η')?;
                            }
                        }
                        GreekVowel::Ι => sink.write_char(if precomposed_diacritics.dialytika {
                            diacritics.dialytika = false;
                            'Ϊ'
                        } else {
                            vowel.into()
                        })?,
                        GreekVowel::Υ => sink.write_char(if precomposed_diacritics.dialytika {
                            diacritics.dialytika = false;
                            'Ϋ'
                        } else {
                            vowel.into()
                        })?,
                        _ => sink.write_char(vowel.into())?,
                    };
                    if diacritics.dialytika {
                        sink.write_char(greek_to_me::DIALYTIKA)?;
                    }
                    if precomposed_diacritics.ypogegrammeni {
                        sink.write_char('Ι')?;
                    }

                    return Ok(());
                }
                // Rho might have breathing marks, we handle it specially
                // to remove them
                Some(GreekPrecomposedLetterData::Consonant(true)) => {
                    sink.write_char(greek_to_me::CAPITAL_RHO)?;
                    return Ok(());
                }
                _ => (),
            }
        }

        let data = self.lookup_data(c);
        if !data.has_exception() {
            if data.is_relevant_to(kind) {
                let mapped = c as i32 + data.delta() as i32;
                // GIGO: delta should be valid
                let mapped = char::from_u32(mapped as u32).unwrap_or(c);
                sink.write_char(mapped)
            } else {
                sink.write_char(c)
            }
        } else {
            let idx = data.exception_index();
            let exception = self.exceptions.get(idx);
            if exception.bits.has_conditional_special() {
                if let Some(special) = match kind {
                    MappingKind::Lower => {
                        self.full_lower_special_case::<IS_TITLE_CONTEXT>(c, context, locale)
                    }
                    MappingKind::Fold => self.full_fold_special_case(c, context, locale),
                    MappingKind::Upper | MappingKind::Title => self
                        .full_upper_or_title_special_case::<IS_TITLE_CONTEXT>(c, context, locale),
                } {
                    return special.write_to(sink);
                }
            }
            if let Some(mapped_string) = exception.get_fullmappings_slot_for_kind(kind) {
                if !mapped_string.is_empty() {
                    return sink.write_str(mapped_string);
                }
            }

            if kind == MappingKind::Fold && exception.bits.no_simple_case_folding() {
                return sink.write_char(c);
            }

            if data.is_relevant_to(kind) {
                if let Some(simple) = exception.get_simple_case_slot_for(c) {
                    return sink.write_char(simple);
                }
            }

            if let Some(slot_char) = exception.slot_char_for_kind(kind) {
                sink.write_char(slot_char)
            } else {
                sink.write_char(c)
            }
        }
    }

    // These constants are used for hardcoded locale-specific foldings.
    const I_DOT: &'static str = "\u{69}\u{307}";
    const J_DOT: &'static str = "\u{6a}\u{307}";
    const I_OGONEK_DOT: &'static str = "\u{12f}\u{307}";
    const I_DOT_GRAVE: &'static str = "\u{69}\u{307}\u{300}";
    const I_DOT_ACUTE: &'static str = "\u{69}\u{307}\u{301}";
    const I_DOT_TILDE: &'static str = "\u{69}\u{307}\u{303}";

    // Special case folding mappings, hardcoded.
    // This handles the special Turkic mappings for uppercase I and dotted uppercase I
    // For non-Turkic languages, this mapping is normally not used.
    // For Turkic languages (tr, az), this mapping can be used instead of the normal mapping for these characters.
    fn simple_fold_special_case(&self, c: char, options: FoldOptions) -> char {
        debug_assert!(c == '\u{49}' || c == '\u{130}');
        let is_turkic = options.exclude_special_i;
        match (c, is_turkic) {
            // Turkic mappings
            ('\u{49}', true) => '\u{131}', // 0049; T; 0131; # LATIN CAPITAL LETTER I
            ('\u{130}', true) => '\u{69}', /* 0130; T; 0069; # LATIN CAPITAL LETTER I WITH DOT ABOVE */

            // Default mappings
            ('\u{49}', false) => '\u{69}', // 0049; C; 0069; # LATIN CAPITAL LETTER I

            // There is no simple case folding for U+130.
            (c, _) => c,
        }
    }

    fn full_lower_special_case<const IS_TITLE_CONTEXT: bool>(
        &self,
        c: char,
        context: ContextIterator,
        locale: CaseMapLocale,
    ) -> Option<FullMappingResult<'_>> {
        if locale == CaseMapLocale::Lithuanian {
            // Lithuanian retains the dot in a lowercase i when followed by accents.
            // Introduce an explicit dot above when lowercasing capital I's and J's
            // whenever there are more accents above (of the accents used in
            // Lithuanian: grave, acute, and tilde above).

            // Check for accents above I, J, and I-with-ogonek.
            if c == 'I' && context.followed_by_more_above(self) {
                return Some(FullMappingResult::String(Self::I_DOT));
            } else if c == 'J' && context.followed_by_more_above(self) {
                return Some(FullMappingResult::String(Self::J_DOT));
            } else if c == '\u{12e}' && context.followed_by_more_above(self) {
                return Some(FullMappingResult::String(Self::I_OGONEK_DOT));
            }

            // These characters are precomposed with accents above, so we don't
            // have to look at the context.
            if c == '\u{cc}' {
                return Some(FullMappingResult::String(Self::I_DOT_GRAVE));
            } else if c == '\u{cd}' {
                return Some(FullMappingResult::String(Self::I_DOT_ACUTE));
            } else if c == '\u{128}' {
                return Some(FullMappingResult::String(Self::I_DOT_TILDE));
            }
        }

        if locale == CaseMapLocale::Turkish {
            if c == '\u{130}' {
                // I and i-dotless; I-dot and i are case pairs in Turkish and Azeri
                return Some(FullMappingResult::CodePoint('i'));
            } else if c == '\u{307}' && context.preceded_by_capital_i::<IS_TITLE_CONTEXT>(self) {
                // When lowercasing, remove dot_above in the sequence I + dot_above,
                // which will turn into i. This matches the behaviour of the
                // canonically equivalent I-dot_above.
                //
                // In a titlecase context, we do not want to apply this behavior to cases where the I
                // was at the beginning of the string, as that I and its marks should be handled by the
                // uppercasing rules (which ignore it, see below)

                return Some(FullMappingResult::Remove);
            } else if c == 'I' && !context.followed_by_dot_above(self) {
                // When lowercasing, unless an I is before a dot_above, it turns
                // into a dotless i.
                return Some(FullMappingResult::CodePoint('\u{131}'));
            }
        }

        if c == '\u{130}' {
            // Preserve canonical equivalence for I with dot. Turkic is handled above.
            return Some(FullMappingResult::String(Self::I_DOT));
        }

        if c == '\u{3a3}'
            && context.preceded_by_cased_letter(self)
            && !context.followed_by_cased_letter(self)
        {
            // Greek capital sigman maps depending on surrounding cased letters.
            return Some(FullMappingResult::CodePoint('\u{3c2}'));
        }

        // No relevant special case mapping. Use a normal mapping.
        None
    }

    fn full_upper_or_title_special_case<const IS_TITLE_CONTEXT: bool>(
        &self,
        c: char,
        context: ContextIterator,
        locale: CaseMapLocale,
    ) -> Option<FullMappingResult<'_>> {
        if locale == CaseMapLocale::Turkish && c == 'i' {
            // In Turkic languages, i turns into a dotted capital I.
            return Some(FullMappingResult::CodePoint('\u{130}'));
        }
        if locale == CaseMapLocale::Lithuanian
            && c == '\u{307}'
            && context.preceded_by_soft_dotted(self)
        {
            // Lithuanian retains the dot in a lowercase i when followed by accents.
            // Remove dot_above after i with upper or titlecase.
            return Some(FullMappingResult::Remove);
        }
        // ICU4C's non-standard extension for Armenian ligature ech-yiwn.
        if c == '\u{587}' {
            return match (locale, IS_TITLE_CONTEXT) {
                (CaseMapLocale::Armenian, false) => Some(FullMappingResult::String("ԵՎ")),
                (CaseMapLocale::Armenian, true) => Some(FullMappingResult::String("Եվ")),
                (_, false) => Some(FullMappingResult::String("ԵՒ")),
                (_, true) => Some(FullMappingResult::String("Եւ")),
            };
        }
        None
    }

    fn full_fold_special_case(
        &self,
        c: char,
        _context: ContextIterator,
        locale: CaseMapLocale,
    ) -> Option<FullMappingResult<'_>> {
        let is_turkic = locale == CaseMapLocale::Turkish;
        match (c, is_turkic) {
            // Turkic mappings
            ('\u{49}', true) => Some(FullMappingResult::CodePoint('\u{131}')),
            ('\u{130}', true) => Some(FullMappingResult::CodePoint('\u{69}')),

            // Default mappings
            ('\u{49}', false) => Some(FullMappingResult::CodePoint('\u{69}')),
            ('\u{130}', false) => Some(FullMappingResult::String(Self::I_DOT)),
            (_, _) => None,
        }
    }
    /// `IS_TITLE_CONTEXT` is true iff the mapping is [`MappingKind::Title`], primarily exists
    /// to avoid perf impacts on other more common modes of operation
    ///
    /// `titlecase_tail_casing` is only read in `IS_TITLE_CONTEXT`
    pub(crate) fn full_helper_writeable<'a: 'data, const IS_TITLE_CONTEXT: bool>(
        &'data self,
        src: &'a str,
        locale: CaseMapLocale,
        mapping: MappingKind,
        titlecase_tail_casing: TrailingCase,
    ) -> FullCaseWriteable<'a, 'data, IS_TITLE_CONTEXT> {
        // Ensure that they are either both true or both false
        debug_assert!(IS_TITLE_CONTEXT == (mapping == MappingKind::Title));

        FullCaseWriteable::<IS_TITLE_CONTEXT> {
            data: self,
            src,
            locale,
            mapping,
            titlecase_tail_casing,
        }
    }

    /// Adds all simple case mappings and the full case folding for `c` to `set`.
    /// Also adds special case closure mappings.
    /// The character itself is not added.
    /// For example, the mappings
    /// - for s include long s
    /// - for sharp s include ss
    /// - for k include the Kelvin sign
    pub(crate) fn add_case_closure_to<S: ClosureSink>(&self, c: char, set: &mut S) {
        // Hardcode the case closure of i and its relatives and ignore the
        // data file data for these characters.
        // The Turkic dotless i and dotted I with their case mapping conditions
        // and case folding option make the related characters behave specially.
        // This code matches their closure behavior to their case folding behavior.
        match c {
            // Regular i and I are in one equivalence class.
            '\u{49}' => {
                set.add_char('\u{69}');
                return;
            }
            '\u{69}' => {
                set.add_char('\u{49}');
                return;
            }

            // Dotted I is in a class with <0069 0307> (for canonical equivalence with <0049 0307>)
            '\u{130}' => {
                set.add_string(Self::I_DOT);
                return;
            }

            // Dotless i is in a class by itself
            '\u{131}' => {
                return;
            }

            _ => {}
        }

        let data = self.lookup_data(c);
        if !data.has_exception() {
            if data.case_type().is_some() {
                let delta = data.delta() as i32;
                if delta != 0 {
                    // Add the one simple case mapping, no matter what type it is.
                    let codepoint = c as i32 + delta;
                    // GIGO: delta should be valid
                    let mapped = char::from_u32(codepoint as u32).unwrap_or(c);
                    set.add_char(mapped);
                }
            }
            return;
        }

        // c has exceptions, so there may be multiple simple and/or full case mappings.
        let idx = data.exception_index();
        let exception = self.exceptions.get(idx);

        // Add all simple case mappings.
        for slot in [
            ExceptionSlot::Lower,
            ExceptionSlot::Fold,
            ExceptionSlot::Upper,
            ExceptionSlot::Title,
        ] {
            if let Some(simple) = exception.get_char_slot(slot) {
                set.add_char(simple);
            }
        }
        if let Some(simple) = exception.get_simple_case_slot_for(c) {
            set.add_char(simple);
        }

        exception.add_full_and_closure_mappings(set);
    }

    /// Maps the string to single code points and adds the associated case closure
    /// mappings.
    ///
    /// (see docs on [`CaseMap::add_string_case_closure_to`])
    pub(crate) fn add_string_case_closure_to<S: ClosureSink>(
        &self,
        s: &str,
        set: &mut S,
        unfold_data: &CaseMapUnfold,
    ) -> bool {
        if s.chars().count() <= 1 {
            // The string is too short to find any match.
            return false;
        }
        match unfold_data.get(s) {
            Some(closure_string) => {
                for c in closure_string.chars() {
                    set.add_char(c);
                    self.add_case_closure_to(c, set);
                }
                true
            }
            None => false,
        }
    }
}

// An internal representation of locale. Non-Root values of this
// enumeration imply that hard-coded special cases exist for this
// language.
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub enum CaseMapLocale {
    Root,
    Turkish,
    Lithuanian,
    Greek,
    Dutch,
    Armenian,
}

impl CaseMapLocale {
    pub const fn from_langid(langid: &LanguageIdentifier) -> Self {
        use icu_locale_core::subtags::{language, Language};
        const TR: Language = language!("tr");
        const AZ: Language = language!("az");
        const LT: Language = language!("lt");
        const EL: Language = language!("el");
        const NL: Language = language!("nl");
        const HY: Language = language!("hy");
        match langid.language {
            TR | AZ => Self::Turkish,
            LT => Self::Lithuanian,
            EL => Self::Greek,
            NL => Self::Dutch,
            HY => Self::Armenian,
            _ => Self::Root,
        }
    }
}

pub enum FullMappingResult<'a> {
    Remove,
    CodePoint(char),
    String(&'a str),
}

impl FullMappingResult<'_> {
    #[allow(dead_code)]
    fn add_to_set<S: ClosureSink>(&self, set: &mut S) {
        match *self {
            FullMappingResult::CodePoint(c) => set.add_char(c),
            FullMappingResult::String(s) => set.add_string(s),
            FullMappingResult::Remove => {}
        }
    }
}

impl Writeable for FullMappingResult<'_> {
    fn write_to<W: fmt::Write + ?Sized>(&self, sink: &mut W) -> fmt::Result {
        match *self {
            FullMappingResult::CodePoint(c) => sink.write_char(c),
            FullMappingResult::String(s) => sink.write_str(s),
            FullMappingResult::Remove => Ok(()),
        }
    }
}

pub(crate) struct ContextIterator<'a> {
    before: &'a str,
    after: &'a str,
}

impl<'a> ContextIterator<'a> {
    // Returns a context iterator with the characters before
    // and after the character at a given index, given the preceding
    // string and the succeeding string including the character itself
    pub fn new(before: &'a str, char_and_after: &'a str) -> Self {
        let mut char_and_after = char_and_after.chars();
        char_and_after.next(); // skip the character itself
        let after = char_and_after.as_str();
        Self { before, after }
    }

    fn add_greek_diacritics(&self, mut diacritics: GreekDiacritics) -> GreekDiacritics {
        diacritics.consume_greek_diacritics(self.after);
        diacritics
    }

    fn preceded_by_greek_letter(&self) -> bool {
        greek_to_me::preceded_by_greek_letter(self.before)
    }

    fn preceding_greek_vowel_diacritics(
        &self,
    ) -> Option<GreekCombiningCharacterSequenceDiacritics> {
        greek_to_me::preceding_greek_vowel_diacritics(self.before)
    }

    fn preceded_by_soft_dotted(&self, mapping: &CaseMap) -> bool {
        for c in self.before.chars().rev() {
            match mapping.dot_type(c) {
                DotType::SoftDotted => return true,
                DotType::OtherAccent => continue,
                _ => return false,
            }
        }
        false
    }
    /// Checks if the preceding character is a capital I, allowing for non-Above combining characters in between.
    ///
    /// If `I_MUST_NOT_START_STRING` is true, additionally will require that the capital I does not start the string
    fn preceded_by_capital_i<const I_MUST_NOT_START_STRING: bool>(
        &self,
        mapping: &CaseMap,
    ) -> bool {
        let mut iter = self.before.chars().rev();
        while let Some(c) = iter.next() {
            if c == 'I' {
                if I_MUST_NOT_START_STRING {
                    return iter.next().is_some();
                } else {
                    return true;
                }
            }
            if mapping.dot_type(c) != DotType::OtherAccent {
                break;
            }
        }
        false
    }
    fn preceded_by_cased_letter(&self, mapping: &CaseMap) -> bool {
        for c in self.before.chars().rev() {
            let data = mapping.lookup_data(c);
            if !data.is_ignorable() {
                return data.case_type().is_some();
            }
        }
        false
    }
    fn followed_by_cased_letter(&self, mapping: &CaseMap) -> bool {
        for c in self.after.chars() {
            let data = mapping.lookup_data(c);
            if !data.is_ignorable() {
                return data.case_type().is_some();
            }
        }
        false
    }
    fn followed_by_more_above(&self, mapping: &CaseMap) -> bool {
        for c in self.after.chars() {
            match mapping.dot_type(c) {
                DotType::Above => return true,
                DotType::OtherAccent => continue,
                _ => return false,
            }
        }
        false
    }
    fn followed_by_dot_above(&self, mapping: &CaseMap) -> bool {
        for c in self.after.chars() {
            if c == '\u{307}' {
                return true;
            }
            if mapping.dot_type(c) != DotType::OtherAccent {
                return false;
            }
        }
        false
    }

    /// Checks the preceding and surrounding context of a j or J
    /// and returns true if it is preceded by an i or I at the start of the string.
    /// If one has an acute accent,
    /// both must have the accent for this to return true. No other accents are handled.
    fn is_dutch_ij_pair_at_beginning(&self, mapping: &CaseMap) -> bool {
        let mut before = self.before.chars().rev();
        let mut i_has_acute = false;
        loop {
            match before.next() {
                Some('i') | Some('I') => break,
                Some('í') | Some('Í') => {
                    i_has_acute = true;
                    break;
                }
                Some(ACUTE) => i_has_acute = true,
                _ => return false,
            }
        }

        if before.next().is_some() {
            // not at the beginning of a string, doesn't matter
            return false;
        }
        let mut j_has_acute = false;
        for c in self.after.chars() {
            if c == ACUTE {
                j_has_acute = true;
                continue;
            }
            // We are supposed to check that `j` has no other combining marks aside
            // from potentially an acute accent. Once we hit the first non-combining mark
            // we are done.
            //
            // ICU4C checks for `gc=Mn` to determine if something is a combining mark,
            // however this requires extra data (and is the *only* point in the casemapping algorithm
            // where there is a direct dependency on properties data not mediated by the casemapping data trie).
            //
            // Instead, we can check for ccc via dot_type, the same way the rest of the algorithm does.
            //
            // See https://unicode-org.atlassian.net/browse/ICU-22429
            match mapping.dot_type(c) {
                // Not a combining character; ccc = 0
                DotType::NoDot | DotType::SoftDotted => break,
                // found combining character, bail
                _ => return false,
            }
        }

        // either both should have an acute accent, or none. this is an XNOR operation
        !(j_has_acute ^ i_has_acute)
    }
}
