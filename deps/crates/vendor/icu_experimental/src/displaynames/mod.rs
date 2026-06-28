// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Display names for languages and regions.
//!
//! There are currently two designs for how to use this component:
//!
//! 1. [`multi`]: Load multiple display names at once.
//! 2. [`single`]: Load a single display name at a time.
//!
//! There are multiple use cases for this component, so we are not yet committed
//! to either of these designs being the "primary" design. Please share feedback at
//! <https://github.com/unicode-org/icu4x/issues/7824>.
//!
//! Note: Currently, the data between the two modules is NOT being shared.
//!
//! ## Examples
//!
//! The [`multi`] module lets you load multiple names at once, whereas [`single`]
//! loads one name at a time.
//!
//! ```
//! use icu::experimental::displaynames::multi::RegionDisplayNames;
//! use icu::experimental::displaynames::single::RegionDisplayName;
//! use icu::experimental::displaynames::DisplayNamesOptions;
//! use icu::locale::{locale, subtags::region};
//! use writeable::assert_writeable_eq;
//!
//! let locale = locale!("en").into();
//!
//! // Multi: Load a formatter that can display many regions.
//! let multi = RegionDisplayNames::try_new(locale, DisplayNamesOptions::default()).unwrap();
//! assert_writeable_eq!(multi.of(region!("US")).unwrap(), "United States");
//! assert_writeable_eq!(multi.of(region!("GB")).unwrap(), "United Kingdom");
//!
//! // Single: Load only the region(s) we need.
//! let us = RegionDisplayName::try_new(locale, region!("US")).unwrap();
//! let gb = RegionDisplayName::try_new(locale, region!("GB")).unwrap();
//! assert_writeable_eq!(us, "United States");
//! assert_writeable_eq!(gb, "United Kingdom");
//! ```

mod displaynames;
mod options;
pub mod provider;
mod singular;

pub mod multi {
    //! Types for loading multiple display names at once.
    //!
    //! This submodule is useful for applications that need to display multiple names
    //! of the same type, such as a list of regions or scripts.
    //!
    //! See [the parent module](mod@super) for a comparison of single and multi.
    use super::displaynames;
    pub use displaynames::LanguageDisplayNames;
    pub use displaynames::LocaleDisplayNamesFormatter;
    pub use displaynames::RegionDisplayNames;
    pub use displaynames::ScriptDisplayNames;
    pub use displaynames::VariantDisplayNames;
}

pub mod single {
    //! Types for loading a single display name at a time.
    //!
    //! This submodule is useful for applications that only need to display one or
    //! two specific names, such as the name of the current region.
    //!
    //! ### Status
    //!
    //! Currently, this module has limited support. It supports regions and scripts,
    //! but support for languages, locales, and variants is currently missing.
    //! More features are on their way.
    //!
    //! If you have any feedback, please let us know at
    //! <https://github.com/unicode-org/icu4x/issues/7825>.
    //!
    //! See [the parent module](mod@super) for a comparison of single and multi.
    use super::singular;
    pub use singular::RegionDisplayName;
    pub use singular::ScriptDisplayName;
}

pub use displaynames::DisplayNamesPreferences;
pub use options::DisplayNamesOptions;
pub use options::Fallback;
pub use options::LanguageDisplay;
pub use options::Style;
