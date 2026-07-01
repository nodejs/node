#[doc(hidden)]
pub use core::clone::Clone;
#[doc(hidden)]
pub use core::cmp::{Eq, PartialEq};
#[doc(hidden)]
pub use core::concat;
#[doc(hidden)]
pub use core::default::Default;
#[doc(hidden)]
pub use core::fmt::Debug;
#[doc(hidden)]
pub use core::hash::{Hash, Hasher};
#[doc(hidden)]
pub use core::marker::Copy;
#[doc(hidden)]
pub use core::option::Option::{None, Some};
#[doc(hidden)]
pub use core::result::Result::{Err, Ok};
#[doc(hidden)]
pub use core::stringify;

#[doc(hidden)]
pub type Formatter<'a> = core::fmt::Formatter<'a>;
#[doc(hidden)]
pub type FmtResult = core::fmt::Result;

#[doc(hidden)]
pub type bool = core::primitive::bool;
#[doc(hidden)]
pub type str = core::primitive::str;

#[cfg(feature = "printing")]
#[doc(hidden)]
pub use quote;

#[doc(hidden)]
pub type Span = proc_macro2::Span;
#[doc(hidden)]
pub type TokenStream2 = proc_macro2::TokenStream;

#[cfg(feature = "parsing")]
#[doc(hidden)]
pub use crate::group::{parse_braces, parse_brackets, parse_parens};

#[doc(hidden)]
pub use crate::span::IntoSpans;

#[cfg(all(feature = "parsing", feature = "printing"))]
#[doc(hidden)]
pub use crate::parse_quote::parse as parse_quote;

#[cfg(feature = "parsing")]
#[doc(hidden)]
pub use crate::token::parsing::{peek_punct, punct as parse_punct};

#[cfg(feature = "printing")]
#[doc(hidden)]
pub use crate::token::printing::punct as print_punct;

#[cfg(feature = "parsing")]
#[doc(hidden)]
pub use crate::token::private::CustomToken;

#[cfg(feature = "proc-macro")]
#[doc(hidden)]
pub type TokenStream = proc_macro::TokenStream;

#[cfg(feature = "printing")]
#[doc(hidden)]
pub use quote::{ToTokens, TokenStreamExt};

#[doc(hidden)]
pub struct private(pub(crate) ());
