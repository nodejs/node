// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::props::{BidiMirroringGlyph, GeneralCategory, Script};
use crate::provider::{PropertyEnumScriptV1, PropertyNameShortScriptV1};
use crate::{
    CodePointMapData, CodePointMapDataBorrowed, PropertyNamesShort, PropertyNamesShortBorrowed,
};
use icu_provider::prelude::*;

use harfbuzz_traits::{GeneralCategoryFunc, MirroringFunc, ScriptFunc};

/// ✨ *Enabled with the `harfbuzz_traits` Cargo feature.*
impl GeneralCategoryFunc for CodePointMapDataBorrowed<'_, GeneralCategory> {
    fn general_category(&self, ch: char) -> harfbuzz_traits::GeneralCategory {
        self.get(ch).into()
    }
}

/// ✨ *Enabled with the `harfbuzz_traits` Cargo feature.*
impl GeneralCategoryFunc for CodePointMapData<GeneralCategory> {
    fn general_category(&self, ch: char) -> harfbuzz_traits::GeneralCategory {
        GeneralCategoryFunc::general_category(&self.as_borrowed(), ch)
    }
}

/// ✨ *Enabled with the `harfbuzz_traits` Cargo feature.*
impl GeneralCategoryFunc for &'_ CodePointMapData<GeneralCategory> {
    fn general_category(&self, ch: char) -> harfbuzz_traits::GeneralCategory {
        GeneralCategoryFunc::general_category(&self.as_borrowed(), ch)
    }
}

/// ✨ *Enabled with the `harfbuzz_traits` Cargo feature.*
impl MirroringFunc for CodePointMapDataBorrowed<'_, BidiMirroringGlyph> {
    fn mirroring(&self, ch: char) -> char {
        self.get(ch).mirroring_glyph.unwrap_or(ch)
    }
}

/// ✨ *Enabled with the `harfbuzz_traits` Cargo feature.*
impl MirroringFunc for CodePointMapData<BidiMirroringGlyph> {
    fn mirroring(&self, ch: char) -> char {
        MirroringFunc::mirroring(&self.as_borrowed(), ch)
    }
}

/// ✨ *Enabled with the `harfbuzz_traits` Cargo feature.*
impl MirroringFunc for &'_ CodePointMapData<BidiMirroringGlyph> {
    fn mirroring(&self, ch: char) -> char {
        MirroringFunc::mirroring(&self.as_borrowed(), ch)
    }
}

/// ✨ *Enabled with the `harfbuzz_traits` Cargo feature.*
impl ScriptFunc for HarfbuzzScriptDataBorrowed<'_> {
    fn script(&self, ch: char) -> [u8; 4] {
        let script = self.script.get(ch);
        self.script_names
            .get_locale_script(script)
            .unwrap_or(icu_locale_core::subtags::script!("Zzzz"))
            .into_raw()
    }
}

/// ✨ *Enabled with the `harfbuzz_traits` Cargo feature.*
impl ScriptFunc for HarfbuzzScriptData {
    fn script(&self, ch: char) -> [u8; 4] {
        ScriptFunc::script(&self.as_borrowed(), ch)
    }
}

/// ✨ *Enabled with the `harfbuzz_traits` Cargo feature.*
impl ScriptFunc for &'_ HarfbuzzScriptData {
    fn script(&self, ch: char) -> [u8; 4] {
        ScriptFunc::script(&self.as_borrowed(), ch)
    }
}

/// ✨ *Enabled with the `harfbuzz_traits` Cargo feature.*
impl From<GeneralCategory> for harfbuzz_traits::GeneralCategory {
    fn from(val: GeneralCategory) -> Self {
        use GeneralCategory::*;
        match val {
            Unassigned => harfbuzz_traits::GeneralCategory::Unassigned,
            UppercaseLetter => harfbuzz_traits::GeneralCategory::UppercaseLetter,
            LowercaseLetter => harfbuzz_traits::GeneralCategory::LowercaseLetter,
            TitlecaseLetter => harfbuzz_traits::GeneralCategory::TitlecaseLetter,
            ModifierLetter => harfbuzz_traits::GeneralCategory::ModifierLetter,
            OtherLetter => harfbuzz_traits::GeneralCategory::OtherLetter,
            NonspacingMark => harfbuzz_traits::GeneralCategory::NonSpacingMark,
            SpacingMark => harfbuzz_traits::GeneralCategory::SpacingMark,
            EnclosingMark => harfbuzz_traits::GeneralCategory::EnclosingMark,
            DecimalNumber => harfbuzz_traits::GeneralCategory::DecimalNumber,
            LetterNumber => harfbuzz_traits::GeneralCategory::LetterNumber,
            OtherNumber => harfbuzz_traits::GeneralCategory::OtherNumber,
            SpaceSeparator => harfbuzz_traits::GeneralCategory::SpaceSeparator,
            LineSeparator => harfbuzz_traits::GeneralCategory::LineSeparator,
            ParagraphSeparator => harfbuzz_traits::GeneralCategory::ParagraphSeparator,
            Control => harfbuzz_traits::GeneralCategory::Control,
            Format => harfbuzz_traits::GeneralCategory::Format,
            PrivateUse => harfbuzz_traits::GeneralCategory::PrivateUse,
            Surrogate => harfbuzz_traits::GeneralCategory::Surrogate,
            DashPunctuation => harfbuzz_traits::GeneralCategory::DashPunctuation,
            OpenPunctuation => harfbuzz_traits::GeneralCategory::OpenPunctuation,
            ClosePunctuation => harfbuzz_traits::GeneralCategory::ClosePunctuation,
            ConnectorPunctuation => harfbuzz_traits::GeneralCategory::ConnectPunctuation,
            InitialPunctuation => harfbuzz_traits::GeneralCategory::InitialPunctuation,
            FinalPunctuation => harfbuzz_traits::GeneralCategory::FinalPunctuation,
            OtherPunctuation => harfbuzz_traits::GeneralCategory::OtherPunctuation,
            MathSymbol => harfbuzz_traits::GeneralCategory::MathSymbol,
            CurrencySymbol => harfbuzz_traits::GeneralCategory::CurrencySymbol,
            ModifierSymbol => harfbuzz_traits::GeneralCategory::ModifierSymbol,
            OtherSymbol => harfbuzz_traits::GeneralCategory::OtherSymbol,
        }
    }
}

/// Harfbuzz data for the [`ScriptFunc`] implementation
///
/// ✨ *Enabled with the `harfbuzz_traits` Cargo feature.*
#[derive(Debug)]
pub struct HarfbuzzScriptDataBorrowed<'a> {
    script: CodePointMapDataBorrowed<'a, Script>,
    script_names: PropertyNamesShortBorrowed<'a, Script>,
}

/// Harfbuzz data for the [`ScriptFunc`] implementation
///
/// ✨ *Enabled with the `harfbuzz_traits` Cargo feature.*
#[derive(Debug)]
pub struct HarfbuzzScriptData {
    script: CodePointMapData<Script>,
    script_names: PropertyNamesShort<Script>,
}

impl HarfbuzzScriptData {
    /// Construct a new [`HarfbuzzScriptData`] using compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    #[cfg(feature = "compiled_data")]
    #[expect(clippy::new_ret_no_self)]
    pub fn new() -> HarfbuzzScriptDataBorrowed<'static> {
        HarfbuzzScriptDataBorrowed {
            script: CodePointMapData::<Script>::new(),
            script_names: PropertyNamesShort::<Script>::new(),
        }
    }

    /// Construct a new [`HarfbuzzScriptData`] from a data provider.
    pub fn try_new_unstable<D>(provider: &D) -> Result<Self, DataError>
    where
        D: DataProvider<PropertyEnumScriptV1> + DataProvider<PropertyNameShortScriptV1> + ?Sized,
    {
        let script_set = CodePointMapData::<Script>::try_new_unstable(provider)?;
        let script_names = PropertyNamesShort::try_new_unstable(provider)?;
        Ok(Self {
            script: script_set,
            script_names,
        })
    }

    #[cfg(feature = "serde")]
    #[doc = icu_provider::gen_buffer_unstable_docs!(BUFFER,Self::try_new_unstable)]
    pub fn try_new_with_buffer_provider(
        provider: &(impl BufferProvider + ?Sized),
    ) -> Result<Self, DataError> {
        Self::try_new_unstable(&provider.as_deserializing())
    }

    fn as_borrowed(&self) -> HarfbuzzScriptDataBorrowed<'_> {
        HarfbuzzScriptDataBorrowed {
            script: self.script.as_borrowed(),
            script_names: self.script_names.as_borrowed(),
        }
    }
}
