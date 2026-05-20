/// As part of the macro expansion and code generation process, Diplomat
/// generates a simplified version of the Rust AST that captures special
/// types such as opaque structs, [`Box`], and [`Result`] with utilities
/// for handling such types.
pub mod attrs;
pub(crate) use attrs::AttrInheritContext;
pub use attrs::Attrs;

mod methods;
pub use methods::{BorrowedParams, Method, Param, SelfParam, TraitSelfParam};

mod modules;
pub use modules::{File, Module};

mod structs;
pub use structs::Struct;

mod opaque;
pub use opaque::OpaqueType;

mod traits;
pub use traits::{Trait, TraitMethod};

mod enums;
pub use enums::Enum;

mod types;
pub use types::{
    CustomType, LifetimeOrigin, ModSymbol, Mutability, PathType, PrimitiveType, StdlibOrDiplomat,
    StringEncoding, TypeName,
};

mod functions;
pub use functions::Function;

pub(crate) mod lifetimes;
pub use lifetimes::{Lifetime, LifetimeEnv, LifetimeTransitivity, NamedLifetime};

mod paths;
pub use paths::Path;

mod idents;
pub use idents::Ident;

mod docs;
pub use docs::{
    DocType, Docs, DocsUrlGenerator, RustLink, RustLinkDisplay,
    TypeReferenceSyntax as DocsTypeReferenceSyntax,
};

mod macros;
pub use macros::{MacroDef, MacroUse, Macros};
