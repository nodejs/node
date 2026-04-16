// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

/// Internal macro used by `enum_keyword` for nesting.
#[macro_export]
#[doc(hidden)]
macro_rules! __enum_keyword_inner {
    ($key:expr, $name:ident, $variant:ident, $value:ident) => {{
        if $value.get_subtag(1).is_some() {
            return Err(Self::Error::InvalidKeywordValue);
        }
        $name::$variant
    }};
    ($key:expr, $name:ident, $variant:ident, $value:ident, $v2:ident, $($subk:expr => $subv:ident),*) => {{
        const _: () = assert!(!matches!($crate::subtags::subtag!($key), TRUE), "true value not allowed with second level");
        if $value.get_subtag(2).is_some() {
            return Err(Self::Error::InvalidKeywordValue);
        }
        $name::$variant(match $value.get_subtag(1) {
            None => None,
            $(
                Some(s) if s == &$crate::subtags::subtag!($subk) => Some($v2::$subv),
            )*
            _ => return Err(Self::Error::InvalidKeywordValue),
        })
    }};
}

/// Macro used to generate a preference keyword as an enum.
///
/// The macro supports single and two subtag enums.
///
/// # Examples
///
/// ```
/// use icu::locale::preferences::extensions::unicode::enum_keyword;
///
/// enum_keyword!(
///     EmojiPresentationStyle {
///         ("emoji" => Emoji),
///         ("text" => Text),
///         ("default" => Default)
/// }, "em");
///
/// enum_keyword!(
///      MetaKeyword {
///         ("normal" => Normal),
///         ("emoji" => Emoji(EmojiPresentationStyle) {
///             ("emoji" => Emoji),
///             ("text" => Text),
///             ("default" => Default)
///         })
/// }, "mk");
/// ```
#[macro_export]
#[doc(hidden)]
macro_rules! __enum_keyword {
    (
        $(#[$doc:meta])*
        $([$derive_attrs:ty])?
        $name:ident {
            $(
                $(#[$variant_doc:meta])*
                $([$variant_attr:ty])?
                $variant:ident $($v2:ident)?
            ),*
        }
    ) => {
        #[non_exhaustive]
        #[derive(Debug, Clone, Eq, PartialEq, Copy, Hash)]
        $(#[derive($derive_attrs)])?
        $(#[$doc])*
        pub enum $name {
            $(
                $(#[$variant_doc])*
                $(#[$variant_attr])?
                $variant $((Option<$v2>))?
            ),*
        }
    };
    ($(#[$doc:meta])*
    $([$derive_attrs:ty])?
    $name:ident {
        $(
            $(#[$variant_doc:meta])*
            $([$variant_attr:ty])?
            ($key:literal => $variant:ident $(($v2:ident) {
                $(
                    ($subk:literal => $subv:ident)
                ),*
            })?)
        ),* $(,)?
    },
    $ext_key:literal
    $(, $input:ident, $aliases:stmt)?
    ) => {
        $crate::__enum_keyword!(
            $(#[$doc])*
            $([$derive_attrs])?
            $name {
                $(
                    $(#[$variant_doc])*
                    $([$variant_attr])?
                    $variant $($v2)?
                ),*
            }
        );

        impl $crate::preferences::PreferenceKey for $name {
            fn unicode_extension_key() -> Option<$crate::extensions::unicode::Key> {
                Some(Self::UNICODE_EXTENSION_KEY)
            }

            fn try_from_key_value(
                key: &$crate::extensions::unicode::Key,
                value: &$crate::extensions::unicode::Value,
            ) -> Result<Option<Self>, $crate::preferences::extensions::unicode::errors::PreferencesParseError> {
                if Self::UNICODE_EXTENSION_KEY == *key {
                    Self::try_from(value).map(Some)
                } else {
                    Ok(None)
                }
            }

            fn unicode_extension_value(&self) -> Option<$crate::extensions::unicode::Value> {
                Some((*self).into())
            }
        }

        impl $name {
            pub(crate) const UNICODE_EXTENSION_KEY: $crate::extensions::unicode::Key = $crate::extensions::unicode::key!($ext_key);
        }

        impl TryFrom<&$crate::extensions::unicode::Value> for $name {
            type Error = $crate::preferences::extensions::unicode::errors::PreferencesParseError;

            fn try_from(value: &$crate::extensions::unicode::Value) -> Result<Self, Self::Error> {
                const TRUE: $crate::subtags::Subtag = $crate::subtags::subtag!("true");

                #[allow(unused_imports)]
                use $crate::extensions::unicode::value;
                $(
                    let $input = value;
                    $aliases
                )?
                Ok(match value.get_subtag(0).copied().unwrap_or(TRUE) {
                    $(
                        s if s == $crate::subtags::subtag!($key) => $crate::__enum_keyword_inner!($key, $name, $variant, value$(, $v2, $($subk => $subv),*)?),
                    )*
                    _ => return Err(Self::Error::InvalidKeywordValue),
                })
            }
        }

        impl From<$name>  for $crate::extensions::unicode::Value {
            fn from(input: $name) -> $crate::extensions::unicode::Value {
                let f;
                #[allow(unused_mut)]
                let mut s = None;
                match input {
                    $(
                        // This is circumventing a limitation of the macro_rules - we need to have a conditional
                        // $()? case here for when the variant has a value, and macro_rules require us to
                        // reference the $v2 inside it, but in match case it becomes a variable, so clippy
                        // complaints.
                        #[allow(non_snake_case)]
                        $name::$variant $(($v2))? => {
                            f = $crate::subtags::subtag!($key);

                            $(
                                if let Some(v2) = $v2 {
                                    match v2 {
                                        $(
                                            $v2::$subv => s = Some($crate::subtags::subtag!($subk)),
                                        )*
                                    }
                                }
                            )?
                        },
                    )*
                }
                if let Some(s) = s {
                    $crate::extensions::unicode::Value::from_two_subtags(f, s)
                } else {
                    $crate::extensions::unicode::Value::from_subtag(Some(f))
                }
            }
        }

        impl $name {
            /// A helper function for displaying as a `&str`.
            pub const fn as_str(&self) -> &'static str {
                match self {
                    $(
                        // This is circumventing a limitation of the macro_rules - we need to have a conditional
                        // $()? case here for when the variant has a value, and macro_rules require us to
                        // reference the $v2 inside it, but in match case it becomes a variable, so clippy
                        // complaints.
                        #[allow(non_snake_case)]
                        Self::$variant $(($v2))? => {
                            $(
                                if let Some(v2) = $v2 {
                                    return match v2 {
                                        $(
                                            $v2::$subv => concat!($key, '-', $subk),
                                        )*
                                    };
                                }
                            )?
                            return $key;
                        },
                    )*
                }
            }
        }
    };
}
pub use __enum_keyword as enum_keyword;

/// ```compile_fail,E0080
/// use icu_locale_core::preferences::extensions::unicode::enum_keyword;
///
/// enum_keyword!(DummySubKeyword { Standard, Rare });
///
/// icu_locale_core::preferences::extensions::unicode::enum_keyword!(DummyKeyword {
///     ("default" => Default),
///     ("true" => Sub(DummySubKeyword) {
///         ("standard" => Standard),
///         ("rare" => Rare)
///     })
/// }, "dk");
/// ```
fn _nested_true() {}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::extensions::unicode;
    use core::str::FromStr;

    #[test]
    fn enum_keywords_test() {
        enum_keyword!(DummyKeyword {
            ("standard" => Standard),
            ("rare" => Rare),
        }, "dk");

        let v = unicode::Value::from_str("standard").unwrap();
        let dk = DummyKeyword::try_from(&v).unwrap();
        assert_eq!(dk, DummyKeyword::Standard);
        assert_eq!(unicode::Value::from(dk), v);

        let v = unicode::Value::from_str("rare").unwrap();
        let dk = DummyKeyword::try_from(&v).unwrap();
        assert_eq!(dk, DummyKeyword::Rare);
        assert_eq!(unicode::Value::from(dk), v);

        let v = unicode::Value::from_str("foo").unwrap();
        let dk = DummyKeyword::try_from(&v);
        dk.unwrap_err();

        assert_eq!(DummyKeyword::Standard.as_str(), "standard");
    }

    #[test]
    fn enum_keywords_test_alias() {
        enum_keyword!(DummyKeyword {
            ("standard" => Standard),
            ("rare" => Rare),
        }, "dk", s, if *s == value!("std") { return Ok(Self::Standard) });

        let v = unicode::Value::from_str("standard").unwrap();
        let dk = DummyKeyword::try_from(&v).unwrap();
        assert_eq!(dk, DummyKeyword::Standard);
        assert_eq!(unicode::Value::from(dk), v);

        let v_alias = unicode::Value::from_str("std").unwrap();
        let dk = DummyKeyword::try_from(&v_alias).unwrap();
        assert_eq!(dk, DummyKeyword::Standard);
        assert_eq!(unicode::Value::from(dk), v);

        let v = unicode::Value::from_str("rare").unwrap();
        let dk = DummyKeyword::try_from(&v).unwrap();
        assert_eq!(dk, DummyKeyword::Rare);
        assert_eq!(unicode::Value::from(dk), v);

        let v = unicode::Value::from_str("foo").unwrap();
        let dk = DummyKeyword::try_from(&v);
        dk.unwrap_err();

        assert_eq!(DummyKeyword::Standard.as_str(), "standard");
    }

    #[test]
    fn enum_keywords_nested_test() {
        enum_keyword!(DummySubKeyword { Standard, Rare });

        enum_keyword!(DummyKeyword {
            ("default" => Default),
            ("sub" => Sub(DummySubKeyword) {
                ("standard" => Standard),
                ("rare" => Rare)
            })
        }, "dk");

        let v = unicode::Value::from_str("default").unwrap();
        let dk = DummyKeyword::try_from(&v).unwrap();
        assert_eq!(dk, DummyKeyword::Default);
        assert_eq!(unicode::Value::from(dk), v);

        let v = unicode::Value::from_str("sub").unwrap();
        let dk = DummyKeyword::try_from(&v).unwrap();
        assert_eq!(dk, DummyKeyword::Sub(None));
        assert_eq!(unicode::Value::from(dk), v);

        let v = unicode::Value::from_str("foo").unwrap();
        let dk = DummyKeyword::try_from(&v);
        dk.unwrap_err();

        let v = unicode::Value::from_str("sub-standard").unwrap();
        let dk = DummyKeyword::try_from(&v).unwrap();
        assert_eq!(dk, DummyKeyword::Sub(Some(DummySubKeyword::Standard)));
        assert_eq!(unicode::Value::from(dk), v);

        let v = unicode::Value::from_str("sub-rare").unwrap();
        let dk = DummyKeyword::try_from(&v).unwrap();
        assert_eq!(dk, DummyKeyword::Sub(Some(DummySubKeyword::Rare)));
        assert_eq!(unicode::Value::from(dk), v);

        let v = unicode::Value::from_str("sub-foo").unwrap();
        let dk = DummyKeyword::try_from(&v);
        dk.unwrap_err();

        assert_eq!(
            DummyKeyword::Sub(Some(DummySubKeyword::Rare)).as_str(),
            "sub-rare"
        );
    }
}
