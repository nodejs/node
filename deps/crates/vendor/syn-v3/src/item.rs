use crate::attr::Attribute;
use crate::data::{Fields, FieldsNamed, Variant};
use crate::derive::{Data, DataEnum, DataStruct, DataUnion, DeriveInput};
#[cfg(feature = "parsing")]
use crate::error::{Error, Result};
use crate::expr::Expr;
use crate::generics::{Generics, TypeParamBound};
use crate::ident::Ident;
use crate::lifetime::Lifetime;
use crate::mac::Macro;
use crate::pat::{Pat, PatType};
use crate::path::Path;
use crate::punctuated::Punctuated;
use crate::restriction::Visibility;
use crate::stmt::Block;
use crate::token;
use crate::ty::{Abi, ReturnType, Type};
use alloc::boxed::Box;
use alloc::vec::Vec;
#[cfg(feature = "parsing")]
use core::mem;
use proc_macro2::TokenStream;

ast_enum_of_structs! {
    /// Things that can appear directly inside of a module or scope.
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: crate::expr::Expr#syntax-tree-enums
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    #[non_exhaustive]
    pub enum Item {
        /// A constant item: `const MAX: u16 = 65535`.
        Const(ItemConst),

        /// An enum definition: `enum Foo<A, B> { A(A), B(B) }`.
        Enum(ItemEnum),

        /// An `extern crate` item: `extern crate serde`.
        ExternCrate(ItemExternCrate),

        /// A free-standing function: `fn process(n: usize) -> Result<()> { ...
        /// }`.
        Fn(ItemFn),

        /// A block of foreign items: `extern "C" { ... }`.
        ForeignMod(ItemForeignMod),

        /// An impl block providing trait or associated items: `impl<A> Trait
        /// for Data<A> { ... }`.
        Impl(ItemImpl),

        /// A macro invocation, which includes `macro_rules!` definitions.
        Macro(ItemMacro),

        /// A module or module declaration: `mod m` or `mod m { ... }`.
        Mod(ItemMod),

        /// A static item: `static BIKE: Shed = Shed(42)`.
        Static(ItemStatic),

        /// A struct definition: `struct Foo<A> { x: A }`.
        Struct(ItemStruct),

        /// A trait definition: `pub trait Iterator { ... }`.
        Trait(ItemTrait),

        /// A trait alias: `pub trait SharableIterator = Iterator + Sync`.
        TraitAlias(ItemTraitAlias),

        /// A type alias: `type Result<T> = core::result::Result<T, MyError>`.
        Type(ItemType),

        /// A union definition: `union Foo<A, B> { x: A, y: B }`.
        Union(ItemUnion),

        /// A use declaration: `use alloc::collections::HashMap`.
        Use(ItemUse),

        /// Tokens forming an item not interpreted by Syn.
        ///
        /// <div class="warning">
        ///
        /// Important: see [Compatibility notes][crate#verbatim-variants].
        ///
        /// </div>
        Verbatim(TokenStream),
    }
}

ast_struct! {
    /// A constant item: `const MAX: u16 = 65535`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ItemConst {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        /// (Non-exhaustive) Additional optional information about a const item.
        pub modifiers: ConstModifiers,
        pub const_token: Token![const],
        pub ident: Ident,
        pub generics: Generics,
        pub colon_token: Token![:],
        pub ty: Box<Type>,
        pub eq_token: Token![=],
        pub expr: Box<Expr>,
        pub semi_token: Token![;],
    }
}

ast_struct! {
    /// Additional optional information about a const item.
    ///
    /// This data structure may grow to accommodate future Rust language
    /// changes.
    #[non_exhaustive]
    pub struct ConstModifiers {
        /// Unstable syntax: [RFC 1210] "Impl specialization"
        ///
        /// [RFC 1210]: https://rust-lang.github.io/rfcs/1210-impl-specialization.html
        pub defaultness: Option<Token![default]>,
    }
}

impl Default for ConstModifiers {
    fn default() -> Self {
        ConstModifiers { defaultness: None }
    }
}

impl ConstModifiers {
    #[cfg(feature = "parsing")]
    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    pub fn require_empty(&self) -> Result<()> {
        let mut result = Ok(());
        if let Some(defaultness) = &self.defaultness {
            let err = Error::new(defaultness.span, "unexpected const item modifier");
            result = Err(err);
        }
        result
    }
}

ast_struct! {
    /// An enum definition: `enum Foo<A, B> { A(A), B(B) }`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ItemEnum {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        pub enum_token: Token![enum],
        pub ident: Ident,
        pub generics: Generics,
        pub brace_token: token::Brace,
        pub variants: Punctuated<Variant, Token![,]>,
    }
}

ast_struct! {
    /// An `extern crate` item: `extern crate serde`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ItemExternCrate {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        pub extern_token: Token![extern],
        pub crate_token: Token![crate],
        pub ident: Ident,
        pub rename: Option<(Token![as], Ident)>,
        pub semi_token: Token![;],
    }
}

ast_struct! {
    /// A free-standing function: `fn process(n: usize) -> Result<()> { ... }`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ItemFn {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        /// (Non-exhaustive) Additional optional information about a function.
        pub modifiers: FnModifiers,
        pub sig: Signature,
        pub block: Box<Block>,
    }
}

ast_struct! {
    /// Additional optional information about a function.
    ///
    /// This data structure may grow to accommodate future Rust language
    /// changes, including the following in-progress RFCs:
    ///
    /// - [RFC 3513] "Generators" (`gen fn`)
    /// - [RFC 3678] "Trait method impl restrictions" (`final fn`)
    /// - [#128044] "Contracts"
    ///
    /// [RFC 3513]: https://github.com/rust-lang/rust/issues/117078
    /// [RFC 3678]: https://rust-lang.github.io/rfcs/3678-final.html
    /// [#128044]: https://github.com/rust-lang/rust/issues/128044
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    #[non_exhaustive]
    pub struct FnModifiers {
        /// Unstable syntax: [RFC 1210] "Impl specialization"
        ///
        /// [RFC 1210]: https://rust-lang.github.io/rfcs/1210-impl-specialization.html
        pub defaultness: Option<Token![default]>,
    }
}

impl Default for FnModifiers {
    fn default() -> Self {
        FnModifiers { defaultness: None }
    }
}

impl FnModifiers {
    #[cfg(feature = "parsing")]
    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    pub fn require_empty(&self) -> Result<()> {
        let mut result = Ok(());
        if let Some(defaultness) = &self.defaultness {
            let err = Error::new(defaultness.span, "unexpected function modifier");
            result = Err(err);
        }
        result
    }
}

ast_struct! {
    /// A block of foreign items: `extern "C" { ... }`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ItemForeignMod {
        pub attrs: Vec<Attribute>,
        pub unsafety: Option<Token![unsafe]>,
        pub abi: Abi,
        pub brace_token: token::Brace,
        pub items: Vec<ForeignItem>,
    }
}

ast_struct! {
    /// An impl block providing trait or associated items: `impl<A> Trait
    /// for Data<A> { ... }`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ItemImpl {
        pub attrs: Vec<Attribute>,
        /// (Non-exhaustive) Additional optional information about an impl.
        pub modifiers: ImplModifiers,
        pub unsafety: Option<Token![unsafe]>,
        pub impl_token: Token![impl],
        pub generics: Generics,
        /// Trait this impl implements.
        pub trait_: Option<(Path, Token![for])>,
        /// The Self type of the impl.
        pub self_ty: Box<Type>,
        pub brace_token: token::Brace,
        pub items: Vec<ImplItem>,
    }
}

ast_struct! {
    /// Additional optional information about an impl.
    ///
    /// This data structure may grow to accommodate future Rust language
    /// changes, including the following in-progress RFCs:
    ///
    /// - [RFC 3762] "Make trait methods callable in const contexts" (`const impl`)
    ///
    /// [RFC 3762]: https://github.com/rust-lang/rfcs/pull/3762
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    #[non_exhaustive]
    pub struct ImplModifiers {
        /// Unstable syntax: [RFC 1210] "Impl specialization"
        ///
        /// [RFC 1210]: https://rust-lang.github.io/rfcs/1210-impl-specialization.html
        pub defaultness: Option<Token![default]>,

        /// Unstable syntax: [#68318] "Negative impls"
        ///
        /// [#68318]: https://github.com/rust-lang/rust/issues/68318
        pub polarity: Option<Token![!]>,
    }
}

impl Default for ImplModifiers {
    fn default() -> Self {
        ImplModifiers {
            defaultness: None,
            polarity: None,
        }
    }
}

impl ImplModifiers {
    #[cfg(feature = "parsing")]
    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    pub fn require_empty(&self) -> Result<()> {
        let mut result = Ok(());
        if let Some(defaultness) = &self.defaultness {
            let err = Error::new(defaultness.span, "unexpected impl modifier");
            result = Err(err);
        }
        if let Some(polarity) = &self.polarity {
            let err = Error::new(polarity.span, "unexpected impl modifier");
            match &mut result {
                Ok(()) => result = Err(err),
                Err(prev) => prev.combine(err),
            }
        }
        result
    }
}

ast_struct! {
    /// A macro invocation, which includes `macro_rules!` definitions.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ItemMacro {
        pub attrs: Vec<Attribute>,
        /// The `example` in `macro_rules! example { ... }`.
        pub ident: Option<Ident>,
        pub mac: Macro,
        pub semi_token: Option<Token![;]>,
    }
}

ast_struct! {
    /// A module or module declaration: `mod m` or `mod m { ... }`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ItemMod {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        pub unsafety: Option<Token![unsafe]>,
        pub mod_token: Token![mod],
        pub ident: Ident,
        pub content: Option<(token::Brace, Vec<Item>)>,
        pub semi: Option<Token![;]>,
    }
}

ast_struct! {
    /// A static item: `static BIKE: Shed = Shed(42)`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ItemStatic {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        pub static_token: Token![static],
        pub mutability: StaticMutability,
        pub ident: Ident,
        pub colon_token: Token![:],
        pub ty: Box<Type>,
        pub eq_token: Token![=],
        pub expr: Box<Expr>,
        pub semi_token: Token![;],
    }
}

ast_struct! {
    /// A struct definition: `struct Foo<A> { x: A }`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ItemStruct {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        pub struct_token: Token![struct],
        pub ident: Ident,
        pub generics: Generics,
        pub fields: Fields,
        pub semi_token: Option<Token![;]>,
    }
}

ast_struct! {
    /// A trait definition: `pub trait Iterator { ... }`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ItemTrait {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        /// (Non-exhaustive) Additional optional information about a trait.
        pub modifiers: TraitModifiers,
        pub unsafety: Option<Token![unsafe]>,
        pub trait_token: Token![trait],
        pub ident: Ident,
        pub generics: Generics,
        pub colon_token: Option<Token![:]>,
        pub supertraits: Punctuated<TypeParamBound, Token![+]>,
        pub brace_token: token::Brace,
        pub items: Vec<TraitItem>,
    }
}

ast_struct! {
    /// Additional optional information about a trait.
    ///
    /// This data structure may grow to accommodate future Rust language
    /// changes, including the following in-progress RFCs:
    ///
    /// - [RFC 3762] "Make trait methods callable in const contexts" (`const trait`)
    ///
    /// [RFC 3762]: https://github.com/rust-lang/rfcs/pull/3762
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    #[non_exhaustive]
    pub struct TraitModifiers {
        /// Unstable syntax: [RFC 127] "Auto traits"
        ///
        /// [RFC 127]: https://github.com/rust-lang/rust/issues/13231
        pub auto_token: Option<Token![auto]>,
    }
}

impl Default for TraitModifiers {
    fn default() -> Self {
        TraitModifiers { auto_token: None }
    }
}

impl TraitModifiers {
    #[cfg(feature = "parsing")]
    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    pub fn require_empty(&self) -> Result<()> {
        let mut result = Ok(());
        if let Some(auto_token) = &self.auto_token {
            let err = Error::new(auto_token.span, "unexpected trait modifier");
            result = Err(err);
        }
        result
    }
}

ast_struct! {
    /// A trait alias: `pub trait SharableIterator = Iterator + Sync`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ItemTraitAlias {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        pub trait_token: Token![trait],
        pub ident: Ident,
        pub generics: Generics,
        pub eq_token: Token![=],
        pub bounds: Punctuated<TypeParamBound, Token![+]>,
        pub semi_token: Token![;],
    }
}

ast_struct! {
    /// A type alias: `type Result<T> = core::result::Result<T, MyError>`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ItemType {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        /// (Non-exhaustive) Additional optional information about a type alias.
        pub modifiers: TypeModifiers,
        pub type_token: Token![type],
        pub ident: Ident,
        pub generics: Generics,
        pub eq_token: Token![=],
        pub ty: Box<Type>,
        pub semi_token: Token![;],
        pub where_clause_placement: WhereClausePlacement,
    }
}

ast_struct! {
    /// Additional optional information about a type alias.
    ///
    /// This data structure may grow to accommodate future Rust language
    /// changes.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    #[non_exhaustive]
    pub struct TypeModifiers {
        /// Unstable syntax: [RFC 1210] "Impl specialization"
        ///
        /// [RFC 1210]: https://rust-lang.github.io/rfcs/1210-impl-specialization.html
        pub defaultness: Option<Token![default]>,
    }
}

impl Default for TypeModifiers {
    fn default() -> Self {
        TypeModifiers { defaultness: None }
    }
}

impl TypeModifiers {
    #[cfg(feature = "parsing")]
    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    pub fn require_empty(&self) -> Result<()> {
        let mut result = Ok(());
        if let Some(defaultness) = &self.defaultness {
            let err = Error::new(defaultness.span, "unexpected type alias modifier");
            result = Err(err);
        }
        result
    }
}

ast_struct! {
    /// A union definition: `union Foo<A, B> { x: A, y: B }`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ItemUnion {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        pub union_token: Token![union],
        pub ident: Ident,
        pub generics: Generics,
        pub fields: FieldsNamed,
    }
}

ast_struct! {
    /// A use declaration: `use alloc::collections::HashMap`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ItemUse {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        pub use_token: Token![use],
        pub leading_colon: Option<Token![::]>,
        pub tree: UseTree,
        pub semi_token: Token![;],
    }
}

impl Item {
    #[cfg(feature = "parsing")]
    pub(crate) fn replace_attrs(&mut self, new: Vec<Attribute>) -> Vec<Attribute> {
        match self {
            Item::Const(ItemConst { attrs, .. })
            | Item::Enum(ItemEnum { attrs, .. })
            | Item::ExternCrate(ItemExternCrate { attrs, .. })
            | Item::Fn(ItemFn { attrs, .. })
            | Item::ForeignMod(ItemForeignMod { attrs, .. })
            | Item::Impl(ItemImpl { attrs, .. })
            | Item::Macro(ItemMacro { attrs, .. })
            | Item::Mod(ItemMod { attrs, .. })
            | Item::Static(ItemStatic { attrs, .. })
            | Item::Struct(ItemStruct { attrs, .. })
            | Item::Trait(ItemTrait { attrs, .. })
            | Item::TraitAlias(ItemTraitAlias { attrs, .. })
            | Item::Type(ItemType { attrs, .. })
            | Item::Union(ItemUnion { attrs, .. })
            | Item::Use(ItemUse { attrs, .. }) => mem::replace(attrs, new),
            Item::Verbatim(_) => Vec::new(),
        }
    }
}

impl From<DeriveInput> for Item {
    fn from(input: DeriveInput) -> Item {
        match input.data {
            Data::Struct(data) => Item::Struct(ItemStruct {
                attrs: input.attrs,
                vis: input.vis,
                struct_token: data.struct_token,
                ident: input.ident,
                generics: input.generics,
                fields: data.fields,
                semi_token: data.semi_token,
            }),
            Data::Enum(data) => Item::Enum(ItemEnum {
                attrs: input.attrs,
                vis: input.vis,
                enum_token: data.enum_token,
                ident: input.ident,
                generics: input.generics,
                brace_token: data.brace_token,
                variants: data.variants,
            }),
            Data::Union(data) => Item::Union(ItemUnion {
                attrs: input.attrs,
                vis: input.vis,
                union_token: data.union_token,
                ident: input.ident,
                generics: input.generics,
                fields: data.fields,
            }),
        }
    }
}

impl From<ItemStruct> for DeriveInput {
    fn from(input: ItemStruct) -> DeriveInput {
        DeriveInput {
            attrs: input.attrs,
            vis: input.vis,
            ident: input.ident,
            generics: input.generics,
            data: Data::Struct(DataStruct {
                struct_token: input.struct_token,
                fields: input.fields,
                semi_token: input.semi_token,
            }),
        }
    }
}

impl From<ItemEnum> for DeriveInput {
    fn from(input: ItemEnum) -> DeriveInput {
        DeriveInput {
            attrs: input.attrs,
            vis: input.vis,
            ident: input.ident,
            generics: input.generics,
            data: Data::Enum(DataEnum {
                enum_token: input.enum_token,
                brace_token: input.brace_token,
                variants: input.variants,
            }),
        }
    }
}

impl From<ItemUnion> for DeriveInput {
    fn from(input: ItemUnion) -> DeriveInput {
        DeriveInput {
            attrs: input.attrs,
            vis: input.vis,
            ident: input.ident,
            generics: input.generics,
            data: Data::Union(DataUnion {
                union_token: input.union_token,
                fields: input.fields,
            }),
        }
    }
}

ast_enum_of_structs! {
    /// A suffix of an import tree in a `use` item: `Type as Renamed` or `*`.
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: crate::expr::Expr#syntax-tree-enums
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub enum UseTree {
        /// A path prefix of imports in a `use` item: `core::...`.
        Path(UsePath),

        /// An identifier imported by a `use` item: `HashMap`.
        Name(UseName),

        /// An renamed identifier imported by a `use` item: `HashMap as Map`.
        Rename(UseRename),

        /// A glob import in a `use` item: `*`.
        Glob(UseGlob),

        /// A braced group of imports in a `use` item: `{A, B, C}`.
        Group(UseGroup),
    }
}

ast_struct! {
    /// A path prefix of imports in a `use` item: `core::...`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct UsePath {
        pub ident: Ident,
        pub colon2_token: Token![::],
        pub tree: Box<UseTree>,
    }
}

ast_struct! {
    /// An identifier imported by a `use` item: `HashMap`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct UseName {
        pub ident: Ident,
    }
}

ast_struct! {
    /// An renamed identifier imported by a `use` item: `HashMap as Map`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct UseRename {
        pub ident: Ident,
        pub as_token: Token![as],
        pub rename: Ident,
    }
}

ast_struct! {
    /// A glob import in a `use` item: `*`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct UseGlob {
        pub star_token: Token![*],
    }
}

ast_struct! {
    /// A braced group of imports in a `use` item: `{A, B, C}`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct UseGroup {
        pub brace_token: token::Brace,
        pub items: Punctuated<UseTree, Token![,]>,
    }
}

ast_enum_of_structs! {
    /// An item within an `extern` block.
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: crate::expr::Expr#syntax-tree-enums
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    #[non_exhaustive]
    pub enum ForeignItem {
        /// A foreign function in an `extern` block.
        Fn(ForeignItemFn),

        /// A foreign static item in an `extern` block: `static ext: u8`.
        Static(ForeignItemStatic),

        /// A foreign type in an `extern` block: `type void`.
        Type(ForeignItemType),

        /// A macro invocation within an extern block.
        Macro(ForeignItemMacro),

        /// Tokens in an `extern` block not interpreted by Syn.
        ///
        /// <div class="warning">
        ///
        /// Important: see [Compatibility notes][crate#verbatim-variants].
        ///
        /// </div>
        Verbatim(TokenStream),
    }
}

ast_struct! {
    /// A foreign function in an `extern` block.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ForeignItemFn {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        /// (Non-exhaustive) Additional optional information about a function.
        pub modifiers: FnModifiers,
        pub sig: Signature,
        pub semi_token: Token![;],
    }
}

ast_struct! {
    /// A foreign static item in an `extern` block: `static ext: u8`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ForeignItemStatic {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        pub safety: Safety,
        pub static_token: Token![static],
        pub mutability: StaticMutability,
        pub ident: Ident,
        pub colon_token: Token![:],
        pub ty: Box<Type>,
        pub semi_token: Token![;],
    }
}

ast_struct! {
    /// A foreign type in an `extern` block: `type void`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ForeignItemType {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        /// (Non-exhaustive) Additional optional information about a type alias.
        pub modifiers: TypeModifiers,
        pub type_token: Token![type],
        pub ident: Ident,
        pub generics: Generics,
        pub semi_token: Token![;],
    }
}

ast_struct! {
    /// A macro invocation within an extern block.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ForeignItemMacro {
        pub attrs: Vec<Attribute>,
        pub mac: Macro,
        pub semi_token: Option<Token![;]>,
    }
}

ast_enum_of_structs! {
    /// An item declaration within the definition of a trait.
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: crate::expr::Expr#syntax-tree-enums
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    #[non_exhaustive]
    pub enum TraitItem {
        /// An associated constant within the definition of a trait.
        Const(TraitItemConst),

        /// An associated function within the definition of a trait.
        Fn(TraitItemFn),

        /// An associated type within the definition of a trait.
        Type(TraitItemType),

        /// A macro invocation within the definition of a trait.
        Macro(TraitItemMacro),

        /// Tokens within the definition of a trait not interpreted by Syn.
        ///
        /// <div class="warning">
        ///
        /// Important: see [Compatibility notes][crate#verbatim-variants].
        ///
        /// </div>
        Verbatim(TokenStream),
    }
}

ast_struct! {
    /// An associated constant within the definition of a trait.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct TraitItemConst {
        pub attrs: Vec<Attribute>,
        /// (Non-exhaustive) Additional optional information about a const item.
        pub modifiers: ConstModifiers,
        pub const_token: Token![const],
        pub ident: Ident,
        pub generics: Generics,
        pub colon_token: Token![:],
        pub ty: Type,
        pub default: Option<(Token![=], Expr)>,
        pub semi_token: Token![;],
    }
}

ast_struct! {
    /// An associated function within the definition of a trait.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct TraitItemFn {
        pub attrs: Vec<Attribute>,
        /// (Non-exhaustive) Additional optional information about a function.
        pub modifiers: FnModifiers,
        pub sig: Signature,
        pub default: Option<Block>,
        pub semi_token: Option<Token![;]>,
    }
}

ast_struct! {
    /// An associated type within the definition of a trait.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct TraitItemType {
        pub attrs: Vec<Attribute>,
        /// (Non-exhaustive) Additional optional information about a type alias.
        pub modifiers: TypeModifiers,
        pub type_token: Token![type],
        pub ident: Ident,
        pub generics: Generics,
        pub colon_token: Option<Token![:]>,
        pub bounds: Punctuated<TypeParamBound, Token![+]>,
        pub default: Option<(Token![=], Type)>,
        pub semi_token: Token![;],
    }
}

ast_struct! {
    /// A macro invocation within the definition of a trait.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct TraitItemMacro {
        pub attrs: Vec<Attribute>,
        pub mac: Macro,
        pub semi_token: Option<Token![;]>,
    }
}

ast_enum_of_structs! {
    /// An item within an impl block.
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: crate::expr::Expr#syntax-tree-enums
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    #[non_exhaustive]
    pub enum ImplItem {
        /// An associated constant within an impl block.
        Const(ImplItemConst),

        /// An associated function within an impl block.
        Fn(ImplItemFn),

        /// An associated type within an impl block.
        Type(ImplItemType),

        /// A macro invocation within an impl block.
        Macro(ImplItemMacro),

        /// Tokens within an impl block not interpreted by Syn.
        ///
        /// <div class="warning">
        ///
        /// Important: see [Compatibility notes][crate#verbatim-variants].
        ///
        /// </div>
        Verbatim(TokenStream),
    }
}

ast_struct! {
    /// An associated constant within an impl block.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ImplItemConst {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        /// (Non-exhaustive) Additional optional information about a const item.
        pub modifiers: ConstModifiers,
        pub const_token: Token![const],
        pub ident: Ident,
        pub generics: Generics,
        pub colon_token: Token![:],
        pub ty: Type,
        pub eq_token: Token![=],
        pub expr: Expr,
        pub semi_token: Token![;],
    }
}

ast_struct! {
    /// An associated function within an impl block.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ImplItemFn {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        /// (Non-exhaustive) Additional optional information about a function.
        pub modifiers: FnModifiers,
        pub sig: Signature,
        pub block: Block,
    }
}

ast_struct! {
    /// An associated type within an impl block.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ImplItemType {
        pub attrs: Vec<Attribute>,
        pub vis: Visibility,
        /// (Non-exhaustive) Additional optional information about a type alias.
        pub modifiers: TypeModifiers,
        pub type_token: Token![type],
        pub ident: Ident,
        pub generics: Generics,
        pub eq_token: Token![=],
        pub ty: Type,
        pub semi_token: Token![;],
    }
}

ast_struct! {
    /// A macro invocation within an impl block.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct ImplItemMacro {
        pub attrs: Vec<Attribute>,
        pub mac: Macro,
        pub semi_token: Option<Token![;]>,
    }
}

ast_struct! {
    /// A function signature in a trait or implementation: `unsafe fn
    /// initialize(&self)`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct Signature {
        pub constness: Option<Token![const]>,
        pub asyncness: Option<Token![async]>,
        pub safety: Safety,
        pub abi: Option<Abi>,
        pub fn_token: Token![fn],
        pub ident: Ident,
        pub generics: Generics,
        pub paren_token: token::Paren,
        pub inputs: Punctuated<FnArg, Token![,]>,
        pub variadic: Option<Variadic>,
        pub output: ReturnType,
    }
}

impl Signature {
    /// A method's `self` receiver, such as `&self` or `self: Box<Self>`.
    pub fn receiver(&self) -> Option<&Receiver> {
        let arg = self.inputs.first()?;
        match arg {
            FnArg::Receiver(receiver) => Some(receiver),
            FnArg::Typed(_) => None,
        }
    }
}

ast_enum! {
    /// Safe, unsafe or default.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub enum Safety {
        /// The item is qualified as `safe`.
        Safe(Token![safe]),
        /// The item is qualified as `unsafe`.
        Unsafe(Token![unsafe]),
        /// The item is not qualified either way.
        Default,
    }
}

impl Default for Safety {
    fn default() -> Self {
        Safety::Default
    }
}

ast_enum_of_structs! {
    /// An argument in a function signature: the `n: usize` in `fn f(n: usize)`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub enum FnArg {
        /// The `self` argument of an associated method.
        Receiver(Receiver),

        /// A function argument accepted by pattern and type.
        Typed(PatType),
    }
}

ast_struct! {
    /// The `self` argument of an associated method.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct Receiver {
        pub attrs: Vec<Attribute>,
        pub mutability: Option<Token![mut]>,
        pub self_token: Token![self],
        pub kind: ReceiverKind,
    }
}

ast_enum! {
    /// Different shorthand and explicit notations for a method receiver.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    #[non_exhaustive]
    pub enum ReceiverKind {
        /// `self` or `mut self`
        Value,
        /// `&self` or `&mut self`
        Reference(Token![&], Option<Lifetime>, Option<Token![mut]>),
        /// `self: Box<Self>`
        Typed(Token![:], Box<Type>),

        // TODO: https://github.com/rust-lang/rust/issues/123076
        // Pin(Token![&], Option<Lifetime>, Token![pin], PointerMutability),
    }
}

ast_struct! {
    /// The variadic argument of a foreign function.
    ///
    /// ```rust
    /// # struct c_char;
    /// # struct c_int;
    /// #
    /// extern "C" {
    ///     fn printf(format: *const c_char, ...) -> c_int;
    ///     //                               ^^^
    /// }
    /// ```
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub struct Variadic {
        pub attrs: Vec<Attribute>,
        pub pat: Option<(Box<Pat>, Token![:])>,
        pub dots: Token![...],
        pub comma: Option<Token![,]>,
    }
}

ast_enum! {
    /// The mutability of an `Item::Static` or `ForeignItem::Static`.
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    #[non_exhaustive]
    pub enum StaticMutability {
        Mut(Token![mut]),
        None,
    }
}

ast_enum! {
    #[cfg_attr(docsrs, doc(cfg(feature = "full")))]
    pub enum WhereClausePlacement {
        /// `type Ty<T> where T: 'static = T;`
        ///
        /// In associated types, this syntax is **deprecated** in favor of the
        /// late placement.
        ///
        /// ```log
        /// warning: where clause not allowed here
        ///   --> src/main.rs
        ///    |
        ///    | impl Trait for Thing { type Ty<T> where T: 'static = T; }
        ///    |                                   ^^^^^^^^^^^^^^^^
        ///    = note: see issue #89122 <https://github.com/rust-lang/rust/issues/89122> for more information
        ///    = note: `#[warn(deprecated_where_clause_location)]` on by default
        /// ```
        Early,

        /// `type Ty<T> = T where T: 'static;`
        ///
        /// In item-level type aliases, this syntax is **unstable**.
        ///
        /// ```log
        /// error: where clauses are not allowed after the type for type aliases
        ///  --> src/main.rs
        ///   |
        ///   | type Ty<T> = T where T: 'static;
        ///   |                ^^^^^^^^^^^^^^^^
        ///   = note: see issue #112792 <https://github.com/rust-lang/rust/issues/112792> for more information
        ///   = help: add `#![feature(lazy_type_alias)]` to the crate attributes to enable
        /// ```
        Late,
    }
}

impl Copy for WhereClausePlacement {}

impl Clone for WhereClausePlacement {
    fn clone(&self) -> Self {
        *self
    }
}

#[cfg(feature = "parsing")]
pub(crate) mod parsing {
    use crate::attr::{self, Attribute};
    use crate::buffer::Cursor;
    use crate::derive;
    use crate::error::{Error, Result};
    use crate::expr::Expr;
    use crate::ext::IdentExt as _;
    use crate::generics::{self, Generics, TypeParamBound};
    use crate::ident::Ident;
    use crate::item::{
        ConstModifiers, FnArg, FnModifiers, ForeignItem, ForeignItemFn, ForeignItemMacro,
        ForeignItemStatic, ForeignItemType, ImplItem, ImplItemConst, ImplItemFn, ImplItemMacro,
        ImplItemType, ImplModifiers, Item, ItemConst, ItemEnum, ItemExternCrate, ItemFn,
        ItemForeignMod, ItemImpl, ItemMacro, ItemMod, ItemStatic, ItemStruct, ItemTrait,
        ItemTraitAlias, ItemType, ItemUnion, ItemUse, Receiver, ReceiverKind, Safety, Signature,
        StaticMutability, TraitItem, TraitItemConst, TraitItemFn, TraitItemMacro, TraitItemType,
        TraitModifiers, TypeModifiers, UseGlob, UseGroup, UseName, UsePath, UseRename, UseTree,
        Variadic, WhereClausePlacement,
    };
    use crate::lifetime::Lifetime;
    use crate::lit::LitStr;
    use crate::mac::{self, Macro};
    use crate::parse::discouraged::Speculative as _;
    use crate::parse::{Parse, ParseStream};
    use crate::pat::{Pat, PatType, PatWild};
    use crate::path::Path;
    use crate::punctuated::Punctuated;
    use crate::restriction::Visibility;
    use crate::stmt::Block;
    use crate::token;
    use crate::ty::{Abi, ReturnType, Type, TypePath};
    use crate::verbatim;
    use alloc::boxed::Box;
    use alloc::vec::Vec;
    use proc_macro2::TokenStream;

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for Item {
        fn parse(input: ParseStream) -> Result<Self> {
            let begin = input.cursor();
            let attrs = input.call(Attribute::parse_outer)?;
            parse_rest_of_item(begin, attrs, input)
        }
    }

    pub(crate) fn parse_rest_of_item(
        begin: Cursor,
        mut attrs: Vec<Attribute>,
        input: ParseStream,
    ) -> Result<Item> {
        let ahead = input.fork();
        let vis: Visibility = ahead.parse()?;

        let lookahead = ahead.lookahead1();
        let allow_safe = false;
        let mut item = if lookahead.peek(Token![fn]) || peek_signature(&ahead, allow_safe) {
            let vis: Visibility = input.parse()?;
            let sig: Signature = input.parse()?;
            if input.peek(Token![;]) {
                input.parse::<Token![;]>()?;
                Ok(Item::Verbatim(verbatim::between(begin, input.cursor())))
            } else {
                parse_rest_of_fn(input, Vec::new(), vis, sig).map(Item::Fn)
            }
        } else if lookahead.peek(Token![extern]) {
            ahead.parse::<Token![extern]>()?;
            let lookahead = ahead.lookahead1();
            if lookahead.peek(Token![crate]) {
                input.parse().map(Item::ExternCrate)
            } else if lookahead.peek(token::Brace) {
                input.parse().map(Item::ForeignMod)
            } else if lookahead.peek(LitStr) {
                ahead.parse::<LitStr>()?;
                let lookahead = ahead.lookahead1();
                if lookahead.peek(token::Brace) {
                    input.parse().map(Item::ForeignMod)
                } else {
                    Err(lookahead.error())
                }
            } else {
                Err(lookahead.error())
            }
        } else if lookahead.peek(Token![use]) {
            let allow_crate_root_in_path = true;
            match parse_item_use(input, allow_crate_root_in_path)? {
                Some(item_use) => Ok(Item::Use(item_use)),
                None => Ok(Item::Verbatim(verbatim::between(begin, input.cursor()))),
            }
        } else if lookahead.peek(Token![static]) {
            let vis = input.parse()?;
            let static_token = input.parse()?;
            let mutability = input.parse()?;
            let ident = input.parse()?;
            if input.peek(Token![=]) {
                input.parse::<Token![=]>()?;
                input.parse::<Expr>()?;
                input.parse::<Token![;]>()?;
                Ok(Item::Verbatim(verbatim::between(begin, input.cursor())))
            } else {
                let colon_token = input.parse()?;
                let ty = input.parse()?;
                if input.peek(Token![;]) {
                    input.parse::<Token![;]>()?;
                    Ok(Item::Verbatim(verbatim::between(begin, input.cursor())))
                } else {
                    Ok(Item::Static(ItemStatic {
                        attrs: Vec::new(),
                        vis,
                        static_token,
                        mutability,
                        ident,
                        colon_token,
                        ty,
                        eq_token: input.parse()?,
                        expr: input.parse()?,
                        semi_token: input.parse()?,
                    }))
                }
            }
        } else if lookahead.peek(Token![const]) {
            let vis: Visibility = input.parse()?;
            let const_token: Token![const] = input.parse()?;
            let lookahead = input.lookahead1();
            if (lookahead.peek(Ident) && !(input.peek(Token![auto]) && input.peek2(Token![trait])))
                || lookahead.peek(Token![_])
            {
                let ident = input.call(Ident::parse_any)?;
                let mut generics: Generics = input.parse()?;
                let colon_token = input.parse()?;
                let ty = input.parse()?;
                let value = if let Some(eq_token) = input.parse::<Option<Token![=]>>()? {
                    let expr: Expr = input.parse()?;
                    Some((eq_token, expr))
                } else {
                    None
                };
                generics.where_clause = input.parse()?;
                let semi_token: Token![;] = input.parse()?;
                match value {
                    Some((eq_token, expr))
                        if generics.lt_token.is_none() && generics.where_clause.is_none() =>
                    {
                        Ok(Item::Const(ItemConst {
                            attrs: Vec::new(),
                            vis,
                            modifiers: ConstModifiers { defaultness: None },
                            const_token,
                            ident,
                            generics,
                            colon_token,
                            ty,
                            eq_token,
                            expr: Box::new(expr),
                            semi_token,
                        }))
                    }
                    _ => Ok(Item::Verbatim(verbatim::between(begin, input.cursor()))),
                }
            } else if lookahead.peek(Token![trait])
                || (lookahead.peek(Token![unsafe]) && !input.peek2(Token![impl]))
                || lookahead.peek(Token![auto])
            {
                let has_impl_restriction = false;
                parse_trait_or_trait_alias(input, Vec::new(), vis, has_impl_restriction)?;
                Ok(Item::Verbatim(verbatim::between(begin, input.cursor())))
            } else if lookahead.peek(Token![impl]) || input.peek(Token![unsafe]) {
                let defaultness = None;
                let unsafety: Option<Token![unsafe]> = input.parse()?;
                let allow_verbatim_impl = true;
                parse_impl(
                    input,
                    Vec::new(),
                    defaultness,
                    Some(const_token),
                    unsafety,
                    allow_verbatim_impl,
                )?;
                Ok(Item::Verbatim(verbatim::between(begin, input.cursor())))
            } else {
                return Err(lookahead.error());
            }
        } else if lookahead.peek(Token![unsafe]) {
            ahead.parse::<Token![unsafe]>()?;
            let lookahead = ahead.lookahead1();
            if lookahead.peek(Token![trait])
                || lookahead.peek(Token![auto]) && ahead.peek2(Token![trait])
            {
                input.parse().map(Item::Trait)
            } else if vis.is_inherited() && lookahead.peek(Token![impl]) {
                let defaultness: Option<Token![default]> = None;
                let constness: Option<Token![const]> = None;
                let unsafety: Token![unsafe] = input.parse()?;
                let allow_verbatim_impl = true;
                if let Some(item) = parse_impl(
                    input,
                    Vec::new(),
                    defaultness,
                    constness,
                    Some(unsafety),
                    allow_verbatim_impl,
                )? {
                    Ok(Item::Impl(item))
                } else {
                    Ok(Item::Verbatim(verbatim::between(begin, input.cursor())))
                }
            } else if lookahead.peek(Token![extern]) {
                input.parse().map(Item::ForeignMod)
            } else if lookahead.peek(Token![mod]) {
                input.parse().map(Item::Mod)
            } else {
                Err(lookahead.error())
            }
        } else if lookahead.peek(Token![mod]) {
            input.parse().map(Item::Mod)
        } else if lookahead.peek(Token![type]) {
            parse_item_type(begin, input)
        } else if lookahead.peek(Token![struct]) {
            input.parse().map(Item::Struct)
        } else if lookahead.peek(Token![enum]) {
            input.parse().map(Item::Enum)
        } else if lookahead.peek(Token![union]) && ahead.peek2(Ident) {
            input.parse().map(Item::Union)
        } else if lookahead.peek(Token![impl])
            && ahead.peek2(token::Paren)
            && (ahead.peek3(Token![const])
                || ahead.peek3(Token![unsafe])
                || ahead.peek3(Token![auto])
                || ahead.peek3(Token![trait]))
        {
            let vis: Visibility = input.parse()?;
            input.parse::<Token![impl]>()?;
            let restriction;
            parenthesized!(restriction in input);
            let lookahead = restriction.lookahead1();
            if lookahead.peek(Token![crate])
                || lookahead.peek(Token![self])
                || lookahead.peek(Token![super])
            {
                Ident::parse_any(&restriction)?;
            } else if lookahead.peek(Token![in]) {
                restriction.parse::<Token![in]>()?;
                Path::parse_mod_style(&restriction)?;
            } else {
                return Err(lookahead.error());
            }
            input.parse::<Option<Token![const]>>()?;
            let has_impl_restriction = true;
            parse_trait_or_trait_alias(input, Vec::new(), vis, has_impl_restriction)?;
            Ok(Item::Verbatim(verbatim::between(begin, input.cursor())))
        } else if lookahead.peek(Token![trait]) {
            let vis: Visibility = input.parse()?;
            let has_impl_restriction = false;
            parse_trait_or_trait_alias(input, Vec::new(), vis, has_impl_restriction)
        } else if lookahead.peek(Token![auto]) && ahead.peek2(Token![trait]) {
            input.parse().map(Item::Trait)
        } else if vis.is_inherited()
            && (ahead.peek(Token![impl])
                || lookahead.peek(Token![default]) && !ahead.peek2(Token![!]))
        {
            let defaultness: Option<Token![default]> = input.parse()?;
            let constness: Option<Token![const]> = input.parse()?;
            let unsafety: Option<Token![unsafe]> = input.parse()?;
            let allow_verbatim_impl = true;
            if let Some(item) = parse_impl(
                input,
                Vec::new(),
                defaultness,
                constness,
                unsafety,
                allow_verbatim_impl,
            )? {
                Ok(Item::Impl(item))
            } else {
                Ok(Item::Verbatim(verbatim::between(begin, input.cursor())))
            }
        } else if lookahead.peek(Token![macro]) {
            input.advance_to(&ahead);
            parse_macro2(begin, vis, input)
        } else if vis.is_inherited()
            && (lookahead.peek(Ident)
                || lookahead.peek(Token![self])
                || lookahead.peek(Token![super])
                || lookahead.peek(Token![crate])
                || lookahead.peek(Token![::]))
        {
            input.parse().map(Item::Macro)
        } else {
            Err(lookahead.error())
        }?;

        attrs.extend(item.replace_attrs(Vec::new()));
        item.replace_attrs(attrs);
        Ok(item)
    }

    struct FlexibleItemType {
        vis: Visibility,
        defaultness: Option<Token![default]>,
        type_token: Token![type],
        ident: Ident,
        generics: Generics,
        colon_token: Option<Token![:]>,
        bounds: Punctuated<TypeParamBound, Token![+]>,
        ty: Option<(Token![=], Type)>,
        semi_token: Token![;],
        where_clause_placement: WhereClausePlacement,
    }

    enum TypeDefaultness {
        Optional,
        Disallowed,
    }

    impl FlexibleItemType {
        fn parse(
            input: ParseStream,
            allow_defaultness: TypeDefaultness,
            default_where_clause_placement: WhereClausePlacement,
        ) -> Result<Self> {
            let vis: Visibility = input.parse()?;
            let defaultness: Option<Token![default]> = match allow_defaultness {
                TypeDefaultness::Optional => input.parse()?,
                TypeDefaultness::Disallowed => None,
            };
            let type_token: Token![type] = input.parse()?;
            let ident: Ident = input.parse()?;
            let mut generics: Generics = input.parse()?;
            let (colon_token, bounds) = Self::parse_optional_bounds(input)?;

            if let WhereClausePlacement::Early = default_where_clause_placement {
                generics.where_clause = input.parse()?;
            }

            let ty = Self::parse_optional_definition(input)?;

            let where_clause_placement =
                parse_late_where_clause(&mut generics, input, default_where_clause_placement)?;

            let semi_token: Token![;] = input.parse()?;

            Ok(FlexibleItemType {
                vis,
                defaultness,
                type_token,
                ident,
                generics,
                colon_token,
                bounds,
                ty,
                semi_token,
                where_clause_placement,
            })
        }

        fn parse_optional_bounds(
            input: ParseStream,
        ) -> Result<(Option<Token![:]>, Punctuated<TypeParamBound, Token![+]>)> {
            let colon_token: Option<Token![:]> = input.parse()?;

            let mut bounds = Punctuated::new();
            if colon_token.is_some() {
                loop {
                    if input.peek(Token![where]) || input.peek(Token![=]) || input.peek(Token![;]) {
                        break;
                    }
                    bounds.push_value({
                        let allow_precise_capture = false;
                        let allow_const = true;
                        TypeParamBound::parse_single(input, allow_precise_capture, allow_const)?
                    });
                    if input.peek(Token![where]) || input.peek(Token![=]) || input.peek(Token![;]) {
                        break;
                    }
                    bounds.push_punct(input.parse::<Token![+]>()?);
                }
            }

            Ok((colon_token, bounds))
        }

        fn parse_optional_definition(input: ParseStream) -> Result<Option<(Token![=], Type)>> {
            let eq_token: Option<Token![=]> = input.parse()?;
            if let Some(eq_token) = eq_token {
                let definition: Type = input.parse()?;
                Ok(Some((eq_token, definition)))
            } else {
                Ok(None)
            }
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ItemMacro {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let path = input.call(Path::parse_mod_style)?;
            let bang_token: Token![!] = input.parse()?;
            let ident: Option<Ident> = if input.peek(Token![try]) {
                input.call(Ident::parse_any).map(Some)
            } else {
                input.parse()
            }?;
            let (delimiter, tokens) = input.call(mac::parse_delimiter)?;
            let semi_token: Option<Token![;]> = if !delimiter.is_brace() {
                Some(input.parse()?)
            } else {
                None
            };
            Ok(ItemMacro {
                attrs,
                ident,
                mac: Macro {
                    path,
                    bang_token,
                    delimiter,
                    tokens,
                },
                semi_token,
            })
        }
    }

    fn parse_macro2(begin: Cursor, _vis: Visibility, input: ParseStream) -> Result<Item> {
        input.parse::<Token![macro]>()?;
        input.parse::<Ident>()?;

        let mut lookahead = input.lookahead1();
        if lookahead.peek(token::Paren) {
            let paren_content;
            parenthesized!(paren_content in input);
            paren_content.parse::<TokenStream>()?;
            lookahead = input.lookahead1();
        }

        if lookahead.peek(token::Brace) {
            let brace_content;
            braced!(brace_content in input);
            brace_content.parse::<TokenStream>()?;
        } else {
            return Err(lookahead.error());
        }

        Ok(Item::Verbatim(verbatim::between(begin, input.cursor())))
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ItemExternCrate {
        fn parse(input: ParseStream) -> Result<Self> {
            Ok(ItemExternCrate {
                attrs: input.call(Attribute::parse_outer)?,
                vis: input.parse()?,
                extern_token: input.parse()?,
                crate_token: input.parse()?,
                ident: {
                    if input.peek(Token![self]) {
                        input.call(Ident::parse_any)?
                    } else {
                        input.parse()?
                    }
                },
                rename: {
                    if input.peek(Token![as]) {
                        let as_token: Token![as] = input.parse()?;
                        let rename: Ident = if input.peek(Token![_]) {
                            Ident::from(input.parse::<Token![_]>()?)
                        } else {
                            input.parse()?
                        };
                        Some((as_token, rename))
                    } else {
                        None
                    }
                },
                semi_token: input.parse()?,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ItemUse {
        fn parse(input: ParseStream) -> Result<Self> {
            let allow_crate_root_in_path = false;
            parse_item_use(input, allow_crate_root_in_path).map(Option::unwrap)
        }
    }

    fn parse_item_use(
        input: ParseStream,
        allow_crate_root_in_path: bool,
    ) -> Result<Option<ItemUse>> {
        let attrs = input.call(Attribute::parse_outer)?;
        let vis: Visibility = input.parse()?;
        let use_token: Token![use] = input.parse()?;
        let leading_colon: Option<Token![::]> = input.parse()?;
        let tree = parse_use_tree(input, allow_crate_root_in_path && leading_colon.is_none())?;
        let semi_token: Token![;] = input.parse()?;

        let tree = match tree {
            Some(tree) => tree,
            None => return Ok(None),
        };

        Ok(Some(ItemUse {
            attrs,
            vis,
            use_token,
            leading_colon,
            tree,
            semi_token,
        }))
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for UseTree {
        fn parse(input: ParseStream) -> Result<UseTree> {
            let allow_crate_root_in_path = false;
            parse_use_tree(input, allow_crate_root_in_path).map(Option::unwrap)
        }
    }

    fn parse_use_tree(
        input: ParseStream,
        allow_crate_root_in_path: bool,
    ) -> Result<Option<UseTree>> {
        let lookahead = input.lookahead1();
        if lookahead.peek(Ident)
            || lookahead.peek(Token![self])
            || lookahead.peek(Token![super])
            || lookahead.peek(Token![crate])
            || lookahead.peek(Token![try])
        {
            let ident = input.call(Ident::parse_any)?;
            if input.peek(Token![::]) {
                Ok(Some(UseTree::Path(UsePath {
                    ident,
                    colon2_token: input.parse()?,
                    tree: Box::new(input.parse()?),
                })))
            } else if input.peek(Token![as]) {
                Ok(Some(UseTree::Rename(UseRename {
                    ident,
                    as_token: input.parse()?,
                    rename: {
                        if input.peek(Ident) {
                            input.parse()?
                        } else if input.peek(Token![_]) {
                            Ident::from(input.parse::<Token![_]>()?)
                        } else {
                            return Err(input.error("expected identifier or underscore"));
                        }
                    },
                })))
            } else {
                Ok(Some(UseTree::Name(UseName { ident })))
            }
        } else if lookahead.peek(Token![*]) {
            Ok(Some(UseTree::Glob(UseGlob {
                star_token: input.parse()?,
            })))
        } else if lookahead.peek(token::Brace) {
            let content;
            let brace_token = braced!(content in input);
            let mut items = Punctuated::new();
            let mut has_any_crate_root_in_path = false;
            loop {
                if content.is_empty() {
                    break;
                }
                let this_tree_starts_with_crate_root =
                    allow_crate_root_in_path && content.parse::<Option<Token![::]>>()?.is_some();
                has_any_crate_root_in_path |= this_tree_starts_with_crate_root;
                match parse_use_tree(
                    &content,
                    allow_crate_root_in_path && !this_tree_starts_with_crate_root,
                )? {
                    Some(tree) if !has_any_crate_root_in_path => items.push_value(tree),
                    _ => has_any_crate_root_in_path = true,
                }
                if content.is_empty() {
                    break;
                }
                let comma: Token![,] = content.parse()?;
                if !has_any_crate_root_in_path {
                    items.push_punct(comma);
                }
            }
            if has_any_crate_root_in_path {
                Ok(None)
            } else {
                Ok(Some(UseTree::Group(UseGroup { brace_token, items })))
            }
        } else {
            Err(lookahead.error())
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ItemStatic {
        fn parse(input: ParseStream) -> Result<Self> {
            Ok(ItemStatic {
                attrs: input.call(Attribute::parse_outer)?,
                vis: input.parse()?,
                static_token: input.parse()?,
                mutability: input.parse()?,
                ident: input.parse()?,
                colon_token: input.parse()?,
                ty: input.parse()?,
                eq_token: input.parse()?,
                expr: input.parse()?,
                semi_token: input.parse()?,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ItemConst {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let vis: Visibility = input.parse()?;
            let const_token: Token![const] = input.parse()?;

            let lookahead = input.lookahead1();
            let ident = if lookahead.peek(Ident) || lookahead.peek(Token![_]) {
                input.call(Ident::parse_any)?
            } else {
                return Err(lookahead.error());
            };

            let colon_token: Token![:] = input.parse()?;
            let ty: Type = input.parse()?;
            let eq_token: Token![=] = input.parse()?;
            let expr: Expr = input.parse()?;
            let semi_token: Token![;] = input.parse()?;

            Ok(ItemConst {
                attrs,
                vis,
                modifiers: ConstModifiers { defaultness: None },
                const_token,
                ident,
                generics: Generics::default(),
                colon_token,
                ty: Box::new(ty),
                eq_token,
                expr: Box::new(expr),
                semi_token,
            })
        }
    }

    fn peek_signature(input: ParseStream, allow_safe: bool) -> bool {
        let fork = input.fork();
        fork.parse::<Option<Token![const]>>().is_ok()
            && fork.parse::<Option<Token![async]>>().is_ok()
            && ((allow_safe && fork.parse::<Option<Token![safe]>>().unwrap().is_some())
                || fork.parse::<Option<Token![unsafe]>>().is_ok())
            && fork.parse::<Option<Abi>>().is_ok()
            && fork.peek(Token![fn])
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for Signature {
        fn parse(input: ParseStream) -> Result<Self> {
            let allow_safe = false;
            parse_signature(input, allow_safe)
        }
    }

    fn parse_signature(input: ParseStream, allow_safe: bool) -> Result<Signature> {
        let constness: Option<Token![const]> = input.parse()?;
        let asyncness: Option<Token![async]> = input.parse()?;
        let safety = if allow_safe {
            Safety::parse_safe_or_unsafe(input)
        } else {
            Safety::parse_unsafe_only(input)
        }?;
        let abi: Option<Abi> = input.parse()?;
        let fn_token: Token![fn] = input.parse()?;
        let ident: Ident = input.parse()?;
        let mut generics: Generics = input.parse()?;

        let content;
        let paren_token = parenthesized!(content in input);
        let (inputs, variadic) = parse_fn_args(&content)?;

        let output: ReturnType = input.parse()?;
        generics.where_clause = input.parse()?;

        Ok(Signature {
            constness,
            asyncness,
            safety,
            abi,
            fn_token,
            ident,
            generics,
            paren_token,
            inputs,
            variadic,
            output,
        })
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Safety {
        /// Parses `safe`, `unsafe`, or default (neither).
        ///
        /// This is appropriate for matching the syntax of `extern` blocks, in
        /// which functions are unsafe by default and require `safe` otherwise.
        ///
        /// ```
        /// unsafe extern "C" {
        ///     fn implicitly_unsafe();
        ///     unsafe fn explicitly_unsafe();
        ///     safe fn explicitly_safe();
        /// }
        /// ```
        pub fn parse_safe_or_unsafe(input: ParseStream) -> Result<Self> {
            if let Some(token) = input.parse::<Option<Token![safe]>>()? {
                Ok(Safety::Safe(token))
            } else {
                Self::parse_unsafe_only(input)
            }
        }

        /// Parses `unsafe` or default (nothing).
        ///
        /// This is appropriate for functions not within an `extern` block,
        /// which are safe by default and cannot be explicitly marked `safe`.
        ///
        /// ```
        /// fn implicitly_safe() {}
        /// unsafe fn explicitly_unsafe() {}
        ///
        /// // safe fn explicitly_safe() {}
        /// // ^^^^ ERROR: items outside of `unsafe extern { }` cannot be declared with `safe` safety qualifier
        /// ```
        pub fn parse_unsafe_only(input: ParseStream) -> Result<Self> {
            if let Some(token) = input.parse::<Option<Token![unsafe]>>()? {
                Ok(Safety::Unsafe(token))
            } else {
                Ok(Safety::Default)
            }
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ItemFn {
        fn parse(input: ParseStream) -> Result<Self> {
            let outer_attrs = input.call(Attribute::parse_outer)?;
            let vis: Visibility = input.parse()?;
            let sig: Signature = input.parse()?;
            parse_rest_of_fn(input, outer_attrs, vis, sig)
        }
    }

    fn parse_rest_of_fn(
        input: ParseStream,
        mut attrs: Vec<Attribute>,
        vis: Visibility,
        sig: Signature,
    ) -> Result<ItemFn> {
        let content;
        let brace_token = braced!(content in input);
        attr::parsing::parse_inner(&content, &mut attrs)?;
        let stmts = content.call(Block::parse_within)?;

        Ok(ItemFn {
            attrs,
            vis,
            modifiers: FnModifiers { defaultness: None },
            sig,
            block: Box::new(Block { brace_token, stmts }),
        })
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for FnArg {
        fn parse(input: ParseStream) -> Result<Self> {
            let allow_variadic = false;
            let attrs = input.call(Attribute::parse_outer)?;
            match parse_fn_arg_or_variadic(input, attrs, allow_variadic)? {
                FnArgOrVariadic::FnArg(arg) => Ok(arg),
                FnArgOrVariadic::Variadic(_) => unreachable!(),
            }
        }
    }

    enum FnArgOrVariadic {
        FnArg(FnArg),
        Variadic(Variadic),
    }

    fn parse_fn_arg_or_variadic(
        input: ParseStream,
        attrs: Vec<Attribute>,
        allow_variadic: bool,
    ) -> Result<FnArgOrVariadic> {
        let ahead = input.fork();
        if let Ok((reference, mutability, self_token)) = parse_receiver_begin(&ahead) {
            input.advance_to(&ahead);
            let mut receiver = parse_rest_of_receiver(reference, mutability, self_token, input)?;
            receiver.attrs = attrs;
            return Ok(FnArgOrVariadic::FnArg(FnArg::Receiver(receiver)));
        }

        // Hack to parse pre-2018 syntax in
        // test/ui/rfc-2565-param-attrs/param-attrs-pretty.rs
        // because the rest of the test case is valuable.
        if input.peek(Ident) && input.peek2(Token![<]) {
            let span = input.span();
            return Ok(FnArgOrVariadic::FnArg(FnArg::Typed(PatType {
                attrs,
                pat: Box::new(Pat::Wild(PatWild {
                    attrs: Vec::new(),
                    underscore_token: Token![_](span),
                })),
                colon_token: Token![:](span),
                ty: input.parse()?,
            })));
        }

        let pat = Box::new(Pat::parse_single(input)?);
        let colon_token: Token![:] = input.parse()?;

        if allow_variadic {
            if let Some(dots) = input.parse::<Option<Token![...]>>()? {
                return Ok(FnArgOrVariadic::Variadic(Variadic {
                    attrs,
                    pat: Some((pat, colon_token)),
                    dots,
                    comma: None,
                }));
            }
        }

        Ok(FnArgOrVariadic::FnArg(FnArg::Typed(PatType {
            attrs,
            pat,
            colon_token,
            ty: input.parse()?,
        })))
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for Receiver {
        fn parse(input: ParseStream) -> Result<Self> {
            let (reference, mutability, self_token) = parse_receiver_begin(input)?;
            parse_rest_of_receiver(reference, mutability, self_token, input)
        }
    }

    fn parse_receiver_begin(
        input: ParseStream,
    ) -> Result<(
        Option<(Token![&], Option<Lifetime>)>,
        Option<Token![mut]>,
        Token![self],
    )> {
        let reference = if input.peek(Token![&]) {
            let ampersand: Token![&] = input.parse()?;
            let lifetime = Lifetime::parse_optional_any(input);
            Some((ampersand, lifetime))
        } else {
            None
        };
        let mutability: Option<Token![mut]> = input.parse()?;
        let self_token: Token![self] = input.parse()?;
        if input.peek(Token![::]) {
            return Err(input.error("expected `:`"));
        }
        Ok((reference, mutability, self_token))
    }

    fn parse_rest_of_receiver(
        reference: Option<(Token![&], Option<Lifetime>)>,
        mut mutability: Option<Token![mut]>,
        self_token: Token![self],
        input: ParseStream,
    ) -> Result<Receiver> {
        let colon_token: Option<Token![:]> = if reference.is_some() {
            None
        } else {
            input.parse()?
        };
        let kind = if let Some(colon_token) = colon_token {
            let ty: Type = input.parse()?;
            ReceiverKind::Typed(colon_token, Box::new(ty))
        } else if let Some((ampersand, lifetime)) = reference {
            ReceiverKind::Reference(ampersand, lifetime, mutability.take())
        } else {
            ReceiverKind::Value
        };
        Ok(Receiver {
            attrs: Vec::new(),
            mutability,
            self_token,
            kind,
        })
    }

    fn parse_fn_args(
        input: ParseStream,
    ) -> Result<(Punctuated<FnArg, Token![,]>, Option<Variadic>)> {
        let mut args = Punctuated::new();
        let mut variadic = None;
        let mut has_receiver = false;

        while !input.is_empty() {
            let attrs = input.call(Attribute::parse_outer)?;

            if let Some(dots) = input.parse::<Option<Token![...]>>()? {
                variadic = Some(Variadic {
                    attrs,
                    pat: None,
                    dots,
                    comma: if input.is_empty() {
                        None
                    } else {
                        Some(input.parse()?)
                    },
                });
                break;
            }

            let allow_variadic = true;
            let arg = match parse_fn_arg_or_variadic(input, attrs, allow_variadic)? {
                FnArgOrVariadic::FnArg(arg) => arg,
                FnArgOrVariadic::Variadic(arg) => {
                    variadic = Some(Variadic {
                        comma: if input.is_empty() {
                            None
                        } else {
                            Some(input.parse()?)
                        },
                        ..arg
                    });
                    break;
                }
            };

            match &arg {
                FnArg::Receiver(receiver) if has_receiver => {
                    return Err(Error::new(
                        receiver.self_token.span,
                        "unexpected second method receiver",
                    ));
                }
                FnArg::Receiver(receiver) if !args.is_empty() => {
                    return Err(Error::new(
                        receiver.self_token.span,
                        "unexpected method receiver",
                    ));
                }
                FnArg::Receiver(_) => has_receiver = true,
                FnArg::Typed(_) => {}
            }
            args.push_value(arg);

            if input.is_empty() {
                break;
            }

            let comma: Token![,] = input.parse()?;
            args.push_punct(comma);
        }

        Ok((args, variadic))
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ItemMod {
        fn parse(input: ParseStream) -> Result<Self> {
            let mut attrs = input.call(Attribute::parse_outer)?;
            let vis: Visibility = input.parse()?;
            let unsafety: Option<Token![unsafe]> = input.parse()?;
            let mod_token: Token![mod] = input.parse()?;
            let ident: Ident = if input.peek(Token![try]) {
                input.call(Ident::parse_any)
            } else {
                input.parse()
            }?;

            let lookahead = input.lookahead1();
            if lookahead.peek(Token![;]) {
                Ok(ItemMod {
                    attrs,
                    vis,
                    unsafety,
                    mod_token,
                    ident,
                    content: None,
                    semi: Some(input.parse()?),
                })
            } else if lookahead.peek(token::Brace) {
                let content;
                let brace_token = braced!(content in input);
                attr::parsing::parse_inner(&content, &mut attrs)?;

                let mut items = Vec::new();
                while !content.is_empty() {
                    items.push(content.parse()?);
                }

                Ok(ItemMod {
                    attrs,
                    vis,
                    unsafety,
                    mod_token,
                    ident,
                    content: Some((brace_token, items)),
                    semi: None,
                })
            } else {
                Err(lookahead.error())
            }
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ItemForeignMod {
        fn parse(input: ParseStream) -> Result<Self> {
            let mut attrs = input.call(Attribute::parse_outer)?;
            let unsafety: Option<Token![unsafe]> = input.parse()?;
            let abi: Abi = input.parse()?;

            let content;
            let brace_token = braced!(content in input);
            attr::parsing::parse_inner(&content, &mut attrs)?;
            let mut items = Vec::new();
            while !content.is_empty() {
                items.push(content.parse()?);
            }

            Ok(ItemForeignMod {
                attrs,
                unsafety,
                abi,
                brace_token,
                items,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ForeignItem {
        fn parse(input: ParseStream) -> Result<Self> {
            let begin = input.cursor();
            let mut attrs = input.call(Attribute::parse_outer)?;
            let ahead = input.fork();
            let vis: Visibility = ahead.parse()?;

            let lookahead = ahead.lookahead1();
            let allow_safe = true;
            let mut item = if lookahead.peek(Token![fn]) || peek_signature(&ahead, allow_safe) {
                let vis: Visibility = input.parse()?;
                let sig = parse_signature(input, allow_safe)?;
                let has_body = input.peek(token::Brace);
                let semi_token: Option<Token![;]> = if has_body {
                    let content;
                    braced!(content in input);
                    content.call(Attribute::parse_inner)?;
                    content.call(Block::parse_within)?;
                    None
                } else {
                    Some(input.parse()?)
                };
                if has_body {
                    Ok(ForeignItem::Verbatim(verbatim::between(
                        begin,
                        input.cursor(),
                    )))
                } else {
                    Ok(ForeignItem::Fn(ForeignItemFn {
                        attrs: Vec::new(),
                        vis,
                        modifiers: FnModifiers { defaultness: None },
                        sig,
                        semi_token: semi_token.unwrap(),
                    }))
                }
            } else if lookahead.peek(Token![static])
                || ((ahead.peek(Token![unsafe]) || ahead.peek(Token![safe]))
                    && ahead.peek2(Token![static]))
            {
                let vis = input.parse()?;
                let safety = Safety::parse_safe_or_unsafe(input)?;
                let static_token = input.parse()?;
                let mutability = input.parse()?;
                let ident = input.parse()?;
                let colon_token = input.parse()?;
                let ty = input.parse()?;
                let has_value = input.peek(Token![=]);
                if has_value {
                    input.parse::<Token![=]>()?;
                    input.parse::<Expr>()?;
                }
                let semi_token: Token![;] = input.parse()?;
                if has_value {
                    Ok(ForeignItem::Verbatim(verbatim::between(
                        begin,
                        input.cursor(),
                    )))
                } else {
                    Ok(ForeignItem::Static(ForeignItemStatic {
                        attrs: Vec::new(),
                        vis,
                        safety,
                        static_token,
                        mutability,
                        ident,
                        colon_token,
                        ty,
                        semi_token,
                    }))
                }
            } else if lookahead.peek(Token![type]) {
                parse_foreign_item_type(begin, input)
            } else if vis.is_inherited()
                && (lookahead.peek(Ident)
                    || lookahead.peek(Token![self])
                    || lookahead.peek(Token![super])
                    || lookahead.peek(Token![crate])
                    || lookahead.peek(Token![::]))
            {
                input.parse().map(ForeignItem::Macro)
            } else {
                Err(lookahead.error())
            }?;

            let item_attrs = match &mut item {
                ForeignItem::Fn(item) => &mut item.attrs,
                ForeignItem::Static(item) => &mut item.attrs,
                ForeignItem::Type(item) => &mut item.attrs,
                ForeignItem::Macro(item) => &mut item.attrs,
                ForeignItem::Verbatim(_) => return Ok(item),
            };
            attrs.append(item_attrs);
            *item_attrs = attrs;

            Ok(item)
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ForeignItemFn {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let vis: Visibility = input.parse()?;
            let allow_safe = true;
            let sig = parse_signature(input, allow_safe)?;
            let semi_token: Token![;] = input.parse()?;
            Ok(ForeignItemFn {
                attrs,
                vis,
                modifiers: FnModifiers { defaultness: None },
                sig,
                semi_token,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ForeignItemStatic {
        fn parse(input: ParseStream) -> Result<Self> {
            Ok(ForeignItemStatic {
                attrs: input.call(Attribute::parse_outer)?,
                vis: input.parse()?,
                safety: input.call(Safety::parse_safe_or_unsafe)?,
                static_token: input.parse()?,
                mutability: input.parse()?,
                ident: input.parse()?,
                colon_token: input.parse()?,
                ty: input.parse()?,
                semi_token: input.parse()?,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ForeignItemType {
        fn parse(input: ParseStream) -> Result<Self> {
            Ok(ForeignItemType {
                attrs: input.call(Attribute::parse_outer)?,
                vis: input.parse()?,
                modifiers: TypeModifiers { defaultness: None },
                type_token: input.parse()?,
                ident: input.parse()?,
                generics: {
                    let mut generics: Generics = input.parse()?;
                    generics.where_clause = input.parse()?;
                    generics
                },
                semi_token: input.parse()?,
            })
        }
    }

    fn parse_foreign_item_type(begin: Cursor, input: ParseStream) -> Result<ForeignItem> {
        let FlexibleItemType {
            vis,
            defaultness: _,
            type_token,
            ident,
            generics,
            colon_token,
            bounds: _,
            ty,
            semi_token,
            where_clause_placement: _,
        } = FlexibleItemType::parse(
            input,
            TypeDefaultness::Disallowed,
            WhereClausePlacement::Early,
        )?;

        if colon_token.is_some() || ty.is_some() {
            Ok(ForeignItem::Verbatim(verbatim::between(
                begin,
                input.cursor(),
            )))
        } else {
            Ok(ForeignItem::Type(ForeignItemType {
                attrs: Vec::new(),
                vis,
                modifiers: TypeModifiers { defaultness: None },
                type_token,
                ident,
                generics,
                semi_token,
            }))
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ForeignItemMacro {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let mac: Macro = input.parse()?;
            let semi_token: Option<Token![;]> = if mac.delimiter.is_brace() {
                None
            } else {
                Some(input.parse()?)
            };
            Ok(ForeignItemMacro {
                attrs,
                mac,
                semi_token,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ItemType {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let vis = input.parse()?;
            let type_token = input.parse()?;
            let ident = input.parse()?;
            let mut generics: Generics = input.parse()?;
            generics.where_clause = input.parse()?;
            let eq_token = input.parse()?;
            let ty = input.parse()?;

            // For item-level type alias, the "Late" placement is unstable and
            // gated by #![feature(lazy_type_alias)]. If no where-clause is
            // present in the input, set placement to "Early" so a macro can
            // most easily add a where clause into a parsed syntax tree without
            // triggering unstable syntax.
            let default_placement = WhereClausePlacement::Early;
            let where_clause_placement =
                parse_late_where_clause(&mut generics, input, default_placement)?;

            let semi_token = input.parse()?;

            Ok(ItemType {
                attrs,
                vis,
                modifiers: TypeModifiers { defaultness: None },
                type_token,
                ident,
                generics,
                eq_token,
                ty,
                semi_token,
                where_clause_placement,
            })
        }
    }

    fn parse_item_type(begin: Cursor, input: ParseStream) -> Result<Item> {
        let FlexibleItemType {
            vis,
            defaultness: _,
            type_token,
            ident,
            generics,
            colon_token,
            bounds: _,
            ty,
            semi_token,
            where_clause_placement,
        } = FlexibleItemType::parse(
            input,
            TypeDefaultness::Disallowed,
            WhereClausePlacement::Early,
        )?;

        let (eq_token, ty) = match ty {
            Some(ty) if colon_token.is_none() => ty,
            _ => return Ok(Item::Verbatim(verbatim::between(begin, input.cursor()))),
        };

        Ok(Item::Type(ItemType {
            attrs: Vec::new(),
            vis,
            modifiers: TypeModifiers { defaultness: None },
            type_token,
            ident,
            generics,
            eq_token,
            ty: Box::new(ty),
            semi_token,
            where_clause_placement,
        }))
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ItemStruct {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let vis = input.parse::<Visibility>()?;
            let struct_token = input.parse::<Token![struct]>()?;
            let ident = input.parse::<Ident>()?;
            let generics = input.parse::<Generics>()?;
            let (where_clause, fields, semi_token) = derive::parsing::data_struct(input)?;
            Ok(ItemStruct {
                attrs,
                vis,
                struct_token,
                ident,
                generics: Generics {
                    where_clause,
                    ..generics
                },
                fields,
                semi_token,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ItemEnum {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let vis = input.parse::<Visibility>()?;
            let enum_token = input.parse::<Token![enum]>()?;
            let ident = input.parse::<Ident>()?;
            let generics = input.parse::<Generics>()?;
            let (where_clause, brace_token, variants) = derive::parsing::data_enum(input)?;
            Ok(ItemEnum {
                attrs,
                vis,
                enum_token,
                ident,
                generics: Generics {
                    where_clause,
                    ..generics
                },
                brace_token,
                variants,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ItemUnion {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let vis = input.parse::<Visibility>()?;
            let union_token = input.parse::<Token![union]>()?;
            let ident = input.parse::<Ident>()?;
            let generics = input.parse::<Generics>()?;
            let (where_clause, fields) = derive::parsing::data_union(input)?;
            Ok(ItemUnion {
                attrs,
                vis,
                union_token,
                ident,
                generics: Generics {
                    where_clause,
                    ..generics
                },
                fields,
            })
        }
    }

    fn parse_trait_or_trait_alias(
        input: ParseStream,
        attrs: Vec<Attribute>,
        vis: Visibility,
        has_impl_restriction: bool,
    ) -> Result<Item> {
        let unsafety: Option<Token![unsafe]> = input.parse()?;
        let auto_token: Option<Token![auto]> = input.parse()?;
        let trait_token: Token![trait] = input.parse()?;
        let ident: Ident = input.parse()?;
        let generics: Generics = input.parse()?;
        let lookahead = input.lookahead1();
        if has_impl_restriction
            || unsafety.is_some()
            || auto_token.is_some()
            || lookahead.peek(token::Brace)
            || lookahead.peek(Token![:])
            || lookahead.peek(Token![where])
        {
            parse_rest_of_trait(
                input,
                attrs,
                vis,
                unsafety,
                auto_token,
                trait_token,
                ident,
                generics,
            )
            .map(Item::Trait)
        } else if lookahead.peek(Token![=]) {
            parse_rest_of_trait_alias(input, attrs, vis, trait_token, ident, generics)
                .map(Item::TraitAlias)
        } else {
            Err(lookahead.error())
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ItemTrait {
        fn parse(input: ParseStream) -> Result<Self> {
            let outer_attrs = input.call(Attribute::parse_outer)?;
            let vis: Visibility = input.parse()?;
            let unsafety: Option<Token![unsafe]> = input.parse()?;
            let auto_token: Option<Token![auto]> = input.parse()?;
            let trait_token: Token![trait] = input.parse()?;
            let ident: Ident = input.parse()?;
            let generics: Generics = input.parse()?;
            parse_rest_of_trait(
                input,
                outer_attrs,
                vis,
                unsafety,
                auto_token,
                trait_token,
                ident,
                generics,
            )
        }
    }

    fn parse_rest_of_trait(
        input: ParseStream,
        mut attrs: Vec<Attribute>,
        vis: Visibility,
        unsafety: Option<Token![unsafe]>,
        auto_token: Option<Token![auto]>,
        trait_token: Token![trait],
        ident: Ident,
        mut generics: Generics,
    ) -> Result<ItemTrait> {
        let colon_token: Option<Token![:]> = input.parse()?;

        let mut supertraits = Punctuated::new();
        if colon_token.is_some() {
            loop {
                if input.peek(Token![where]) || input.peek(token::Brace) {
                    break;
                }
                supertraits.push_value({
                    let allow_precise_capture = false;
                    let allow_const = true;
                    TypeParamBound::parse_single(input, allow_precise_capture, allow_const)?
                });
                if input.peek(Token![where]) || input.peek(token::Brace) {
                    break;
                }
                supertraits.push_punct(input.parse()?);
            }
        }

        generics.where_clause = input.parse()?;

        let content;
        let brace_token = braced!(content in input);
        attr::parsing::parse_inner(&content, &mut attrs)?;
        let mut items = Vec::new();
        while !content.is_empty() {
            items.push(content.parse()?);
        }

        Ok(ItemTrait {
            attrs,
            vis,
            modifiers: TraitModifiers { auto_token },
            unsafety,
            trait_token,
            ident,
            generics,
            colon_token,
            supertraits,
            brace_token,
            items,
        })
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ItemTraitAlias {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let vis: Visibility = input.parse()?;
            let trait_token: Token![trait] = input.parse()?;
            let ident: Ident = input.parse()?;
            let generics: Generics = input.parse()?;
            parse_rest_of_trait_alias(input, attrs, vis, trait_token, ident, generics)
        }
    }

    fn parse_rest_of_trait_alias(
        input: ParseStream,
        attrs: Vec<Attribute>,
        vis: Visibility,
        trait_token: Token![trait],
        ident: Ident,
        mut generics: Generics,
    ) -> Result<ItemTraitAlias> {
        let eq_token: Token![=] = input.parse()?;

        let mut bounds = Punctuated::new();
        loop {
            if input.peek(Token![where]) || input.peek(Token![;]) {
                break;
            }
            bounds.push_value({
                let allow_precise_capture = false;
                let allow_const = true;
                TypeParamBound::parse_single(input, allow_precise_capture, allow_const)?
            });
            if input.peek(Token![where]) || input.peek(Token![;]) {
                break;
            }
            bounds.push_punct(input.parse()?);
        }

        generics.where_clause = input.parse()?;
        let semi_token: Token![;] = input.parse()?;

        Ok(ItemTraitAlias {
            attrs,
            vis,
            trait_token,
            ident,
            generics,
            eq_token,
            bounds,
            semi_token,
        })
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TraitItem {
        fn parse(input: ParseStream) -> Result<Self> {
            let begin = input.cursor();
            let mut attrs = input.call(Attribute::parse_outer)?;
            let vis: Visibility = input.parse()?;
            let defaultness: Option<Token![default]> = input.parse()?;
            let ahead = input.fork();

            let lookahead = ahead.lookahead1();
            let allow_safe = false;
            let mut item = if lookahead.peek(Token![fn]) || peek_signature(&ahead, allow_safe) {
                input.parse().map(TraitItem::Fn)
            } else if lookahead.peek(Token![const]) {
                let const_token: Token![const] = ahead.parse()?;
                let lookahead = ahead.lookahead1();
                if lookahead.peek(Ident) || lookahead.peek(Token![_]) {
                    input.advance_to(&ahead);
                    let ident = input.call(Ident::parse_any)?;
                    let mut generics: Generics = input.parse()?;
                    let colon_token: Token![:] = input.parse()?;
                    let ty: Type = input.parse()?;
                    let default = if let Some(eq_token) = input.parse::<Option<Token![=]>>()? {
                        let expr: Expr = input.parse()?;
                        Some((eq_token, expr))
                    } else {
                        None
                    };
                    generics.where_clause = input.parse()?;
                    let semi_token: Token![;] = input.parse()?;
                    if generics.lt_token.is_none() && generics.where_clause.is_none() {
                        Ok(TraitItem::Const(TraitItemConst {
                            attrs: Vec::new(),
                            modifiers: ConstModifiers { defaultness: None },
                            const_token,
                            ident,
                            generics,
                            colon_token,
                            ty,
                            default,
                            semi_token,
                        }))
                    } else {
                        return Ok(TraitItem::Verbatim(verbatim::between(
                            begin,
                            input.cursor(),
                        )));
                    }
                } else if lookahead.peek(Token![async])
                    || lookahead.peek(Token![unsafe])
                    || lookahead.peek(Token![extern])
                    || lookahead.peek(Token![fn])
                {
                    input.parse().map(TraitItem::Fn)
                } else {
                    Err(lookahead.error())
                }
            } else if lookahead.peek(Token![type]) {
                parse_trait_item_type(begin, input)
            } else if vis.is_inherited()
                && defaultness.is_none()
                && (lookahead.peek(Ident)
                    || lookahead.peek(Token![self])
                    || lookahead.peek(Token![super])
                    || lookahead.peek(Token![crate])
                    || lookahead.peek(Token![::]))
            {
                input.parse().map(TraitItem::Macro)
            } else {
                Err(lookahead.error())
            }?;

            match (vis, defaultness) {
                (Visibility::Inherited, None) => {}
                _ => {
                    return Ok(TraitItem::Verbatim(verbatim::between(
                        begin,
                        input.cursor(),
                    )))
                }
            }

            let item_attrs = match &mut item {
                TraitItem::Const(item) => &mut item.attrs,
                TraitItem::Fn(item) => &mut item.attrs,
                TraitItem::Type(item) => &mut item.attrs,
                TraitItem::Macro(item) => &mut item.attrs,
                TraitItem::Verbatim(_) => unreachable!(),
            };
            attrs.append(item_attrs);
            *item_attrs = attrs;
            Ok(item)
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TraitItemConst {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let const_token: Token![const] = input.parse()?;

            let lookahead = input.lookahead1();
            let ident = if lookahead.peek(Ident) || lookahead.peek(Token![_]) {
                input.call(Ident::parse_any)?
            } else {
                return Err(lookahead.error());
            };

            let colon_token: Token![:] = input.parse()?;
            let ty: Type = input.parse()?;
            let default = if input.peek(Token![=]) {
                let eq_token: Token![=] = input.parse()?;
                let default: Expr = input.parse()?;
                Some((eq_token, default))
            } else {
                None
            };
            let semi_token: Token![;] = input.parse()?;

            Ok(TraitItemConst {
                attrs,
                modifiers: ConstModifiers { defaultness: None },
                const_token,
                ident,
                generics: Generics::default(),
                colon_token,
                ty,
                default,
                semi_token,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TraitItemFn {
        fn parse(input: ParseStream) -> Result<Self> {
            let mut attrs = input.call(Attribute::parse_outer)?;
            let sig: Signature = input.parse()?;

            let lookahead = input.lookahead1();
            let (brace_token, stmts, semi_token) = if lookahead.peek(token::Brace) {
                let content;
                let brace_token = braced!(content in input);
                attr::parsing::parse_inner(&content, &mut attrs)?;
                let stmts = content.call(Block::parse_within)?;
                (Some(brace_token), stmts, None)
            } else if lookahead.peek(Token![;]) {
                let semi_token: Token![;] = input.parse()?;
                (None, Vec::new(), Some(semi_token))
            } else {
                return Err(lookahead.error());
            };

            Ok(TraitItemFn {
                attrs,
                modifiers: FnModifiers { defaultness: None },
                sig,
                default: brace_token.map(|brace_token| Block { brace_token, stmts }),
                semi_token,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TraitItemType {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let type_token: Token![type] = input.parse()?;
            let ident: Ident = input.parse()?;
            let mut generics: Generics = input.parse()?;
            let (colon_token, bounds) = FlexibleItemType::parse_optional_bounds(input)?;
            let default = FlexibleItemType::parse_optional_definition(input)?;
            generics.where_clause = input.parse()?;
            let semi_token: Token![;] = input.parse()?;
            Ok(TraitItemType {
                attrs,
                modifiers: TypeModifiers { defaultness: None },
                type_token,
                ident,
                generics,
                colon_token,
                bounds,
                default,
                semi_token,
            })
        }
    }

    fn parse_trait_item_type(begin: Cursor, input: ParseStream) -> Result<TraitItem> {
        let FlexibleItemType {
            vis,
            defaultness: _,
            type_token,
            ident,
            generics,
            colon_token,
            bounds,
            ty,
            semi_token,
            where_clause_placement: _,
        } = FlexibleItemType::parse(
            input,
            TypeDefaultness::Disallowed,
            WhereClausePlacement::Late,
        )?;

        if vis.is_some() {
            Ok(TraitItem::Verbatim(verbatim::between(
                begin,
                input.cursor(),
            )))
        } else {
            Ok(TraitItem::Type(TraitItemType {
                attrs: Vec::new(),
                modifiers: TypeModifiers { defaultness: None },
                type_token,
                ident,
                generics,
                colon_token,
                bounds,
                default: ty,
                semi_token,
            }))
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TraitItemMacro {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let mac: Macro = input.parse()?;
            let semi_token: Option<Token![;]> = if mac.delimiter.is_brace() {
                None
            } else {
                Some(input.parse()?)
            };
            Ok(TraitItemMacro {
                attrs,
                mac,
                semi_token,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ItemImpl {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let defaultness: Option<Token![default]> = input.parse()?;
            let constness: Option<Token![const]> = None;
            let unsafety: Option<Token![unsafe]> = input.parse()?;

            let allow_verbatim_impl = false;
            parse_impl(
                input,
                attrs,
                defaultness,
                constness,
                unsafety,
                allow_verbatim_impl,
            )
            .map(Option::unwrap)
        }
    }

    fn parse_impl(
        input: ParseStream,
        mut attrs: Vec<Attribute>,
        defaultness: Option<Token![default]>,
        constness: Option<Token![const]>,
        unsafety: Option<Token![unsafe]>,
        allow_verbatim_impl: bool,
    ) -> Result<Option<ItemImpl>> {
        let impl_token: Token![impl] = input.parse()?;

        let has_generics = generics::parsing::choose_generics_over_qpath(input);
        let mut generics: Generics = if has_generics {
            input.parse()?
        } else {
            Generics::default()
        };

        let polarity = if input.peek(Token![!]) && !input.peek2(token::Brace) {
            Some(input.parse::<Token![!]>()?)
        } else {
            None
        };

        let first_ty_begin = input.cursor();
        let mut first_ty: Type = input.parse()?;
        let first_ty_end = input.cursor();
        let self_ty: Type;
        let trait_;

        let is_impl_for = input.peek(Token![for]);
        if is_impl_for {
            let for_token: Token![for] = input.parse()?;
            let mut first_ty_ref = &first_ty;
            while let Type::Group(ty) = first_ty_ref {
                first_ty_ref = &ty.elem;
            }
            if let Type::Path(TypePath {
                attrs: _,
                qself: None,
                ..
            }) = first_ty_ref
            {
                while let Type::Group(ty) = first_ty {
                    first_ty = *ty.elem;
                }
                if let Type::Path(TypePath {
                    attrs: _,
                    qself: None,
                    path,
                }) = first_ty
                {
                    trait_ = Some((path, for_token));
                } else {
                    unreachable!();
                }
            } else if !allow_verbatim_impl {
                return Err(Error::new_range(
                    first_ty_begin..first_ty_end,
                    "expected trait path",
                ));
            } else {
                trait_ = None;
            }
            self_ty = input.parse()?;
        } else if let Some(polarity) = polarity {
            return Err(Error::new(
                polarity.span,
                "inherent impls cannot be negative",
            ));
        } else {
            trait_ = None;
            self_ty = first_ty;
        }

        generics.where_clause = input.parse()?;

        let content;
        let brace_token = braced!(content in input);
        attr::parsing::parse_inner(&content, &mut attrs)?;

        let mut items = Vec::new();
        while !content.is_empty() {
            items.push(content.parse()?);
        }

        if constness.is_some() || is_impl_for && trait_.is_none() {
            Ok(None)
        } else {
            Ok(Some(ItemImpl {
                attrs,
                modifiers: ImplModifiers {
                    defaultness,
                    polarity,
                },
                unsafety,
                impl_token,
                generics,
                trait_,
                self_ty: Box::new(self_ty),
                brace_token,
                items,
            }))
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ImplItem {
        fn parse(input: ParseStream) -> Result<Self> {
            let begin = input.cursor();
            let mut attrs = input.call(Attribute::parse_outer)?;
            let ahead = input.fork();
            let vis: Visibility = ahead.parse()?;

            let mut lookahead = ahead.lookahead1();
            let defaultness = if lookahead.peek(Token![default]) && !ahead.peek2(Token![!]) {
                let defaultness: Token![default] = ahead.parse()?;
                lookahead = ahead.lookahead1();
                Some(defaultness)
            } else {
                None
            };

            let allow_safe = false;
            let mut item = if lookahead.peek(Token![fn]) || peek_signature(&ahead, allow_safe) {
                let allow_omitted_body = true;
                if let Some(item) = parse_impl_item_fn(input, allow_omitted_body)? {
                    Ok(ImplItem::Fn(item))
                } else {
                    Ok(ImplItem::Verbatim(verbatim::between(begin, input.cursor())))
                }
            } else if lookahead.peek(Token![const]) {
                input.advance_to(&ahead);
                let const_token: Token![const] = input.parse()?;
                let lookahead = input.lookahead1();
                let ident = if lookahead.peek(Ident) || lookahead.peek(Token![_]) {
                    input.call(Ident::parse_any)?
                } else {
                    return Err(lookahead.error());
                };
                let mut generics: Generics = input.parse()?;
                let colon_token: Token![:] = input.parse()?;
                let ty: Type = input.parse()?;
                let value = if let Some(eq_token) = input.parse::<Option<Token![=]>>()? {
                    let expr: Expr = input.parse()?;
                    Some((eq_token, expr))
                } else {
                    None
                };
                generics.where_clause = input.parse()?;
                let semi_token: Token![;] = input.parse()?;
                return match value {
                    Some((eq_token, expr))
                        if generics.lt_token.is_none() && generics.where_clause.is_none() =>
                    {
                        Ok(ImplItem::Const(ImplItemConst {
                            attrs,
                            vis,
                            modifiers: ConstModifiers { defaultness },
                            const_token,
                            ident,
                            generics,
                            colon_token,
                            ty,
                            eq_token,
                            expr,
                            semi_token,
                        }))
                    }
                    _ => Ok(ImplItem::Verbatim(verbatim::between(begin, input.cursor()))),
                };
            } else if lookahead.peek(Token![type]) {
                parse_impl_item_type(begin, input)
            } else if vis.is_inherited()
                && defaultness.is_none()
                && (lookahead.peek(Ident)
                    || lookahead.peek(Token![self])
                    || lookahead.peek(Token![super])
                    || lookahead.peek(Token![crate])
                    || lookahead.peek(Token![::]))
            {
                input.parse().map(ImplItem::Macro)
            } else {
                Err(lookahead.error())
            }?;

            {
                let item_attrs = match &mut item {
                    ImplItem::Const(item) => &mut item.attrs,
                    ImplItem::Fn(item) => &mut item.attrs,
                    ImplItem::Type(item) => &mut item.attrs,
                    ImplItem::Macro(item) => &mut item.attrs,
                    ImplItem::Verbatim(_) => return Ok(item),
                };
                attrs.append(item_attrs);
                *item_attrs = attrs;
            }

            Ok(item)
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ImplItemConst {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let vis: Visibility = input.parse()?;
            let defaultness: Option<Token![default]> = input.parse()?;
            let const_token: Token![const] = input.parse()?;

            let lookahead = input.lookahead1();
            let ident = if lookahead.peek(Ident) || lookahead.peek(Token![_]) {
                input.call(Ident::parse_any)?
            } else {
                return Err(lookahead.error());
            };

            let colon_token: Token![:] = input.parse()?;
            let ty: Type = input.parse()?;
            let eq_token: Token![=] = input.parse()?;
            let expr: Expr = input.parse()?;
            let semi_token: Token![;] = input.parse()?;

            Ok(ImplItemConst {
                attrs,
                vis,
                modifiers: ConstModifiers { defaultness },
                const_token,
                ident,
                generics: Generics::default(),
                colon_token,
                ty,
                eq_token,
                expr,
                semi_token,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ImplItemFn {
        fn parse(input: ParseStream) -> Result<Self> {
            let allow_omitted_body = false;
            parse_impl_item_fn(input, allow_omitted_body).map(Option::unwrap)
        }
    }

    fn parse_impl_item_fn(
        input: ParseStream,
        allow_omitted_body: bool,
    ) -> Result<Option<ImplItemFn>> {
        let mut attrs = input.call(Attribute::parse_outer)?;
        let vis: Visibility = input.parse()?;
        let defaultness: Option<Token![default]> = input.parse()?;
        let sig: Signature = input.parse()?;

        // Accept functions without a body in an impl block because rustc's
        // *parser* does not reject them (the compilation error is emitted later
        // than parsing) and it can be useful for macro DSLs.
        if allow_omitted_body && input.parse::<Option<Token![;]>>()?.is_some() {
            return Ok(None);
        }

        let content;
        let brace_token = braced!(content in input);
        attrs.extend(content.call(Attribute::parse_inner)?);
        let block = Block {
            brace_token,
            stmts: content.call(Block::parse_within)?,
        };

        Ok(Some(ImplItemFn {
            attrs,
            vis,
            modifiers: FnModifiers { defaultness },
            sig,
            block,
        }))
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ImplItemType {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let vis: Visibility = input.parse()?;
            let defaultness: Option<Token![default]> = input.parse()?;
            let type_token: Token![type] = input.parse()?;
            let ident: Ident = input.parse()?;
            let mut generics: Generics = input.parse()?;
            let eq_token: Token![=] = input.parse()?;
            let ty: Type = input.parse()?;
            generics.where_clause = input.parse()?;
            let semi_token: Token![;] = input.parse()?;
            Ok(ImplItemType {
                attrs,
                vis,
                modifiers: TypeModifiers { defaultness },
                type_token,
                ident,
                generics,
                eq_token,
                ty,
                semi_token,
            })
        }
    }

    fn parse_impl_item_type(begin: Cursor, input: ParseStream) -> Result<ImplItem> {
        let FlexibleItemType {
            vis,
            defaultness,
            type_token,
            ident,
            generics,
            colon_token,
            bounds: _,
            ty,
            semi_token,
            where_clause_placement: _,
        } = FlexibleItemType::parse(input, TypeDefaultness::Optional, WhereClausePlacement::Late)?;

        let (eq_token, ty) = match ty {
            Some(ty) if colon_token.is_none() => ty,
            _ => return Ok(ImplItem::Verbatim(verbatim::between(begin, input.cursor()))),
        };

        Ok(ImplItem::Type(ImplItemType {
            attrs: Vec::new(),
            vis,
            modifiers: TypeModifiers { defaultness },
            type_token,
            ident,
            generics,
            eq_token,
            ty,
            semi_token,
        }))
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ImplItemMacro {
        fn parse(input: ParseStream) -> Result<Self> {
            let attrs = input.call(Attribute::parse_outer)?;
            let mac: Macro = input.parse()?;
            let semi_token: Option<Token![;]> = if mac.delimiter.is_brace() {
                None
            } else {
                Some(input.parse()?)
            };
            Ok(ImplItemMacro {
                attrs,
                mac,
                semi_token,
            })
        }
    }

    impl Visibility {
        fn is_inherited(&self) -> bool {
            match self {
                Visibility::Inherited => true,
                _ => false,
            }
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for StaticMutability {
        fn parse(input: ParseStream) -> Result<Self> {
            let mut_token: Option<Token![mut]> = input.parse()?;
            Ok(mut_token.map_or(StaticMutability::None, StaticMutability::Mut))
        }
    }

    fn parse_late_where_clause(
        generics: &mut Generics,
        input: ParseStream,
        default_placement: WhereClausePlacement,
    ) -> Result<WhereClausePlacement> {
        if generics.where_clause.is_some() {
            return Ok(WhereClausePlacement::Early);
        }

        generics.where_clause = input.parse()?;
        if generics.where_clause.is_some() {
            Ok(WhereClausePlacement::Late)
        } else {
            Ok(default_placement)
        }
    }
}

#[cfg(feature = "printing")]
mod printing {
    use crate::attr::FilterAttrs;
    use crate::data::Fields;
    use crate::item::{
        ForeignItemFn, ForeignItemMacro, ForeignItemStatic, ForeignItemType, ImplItemConst,
        ImplItemFn, ImplItemMacro, ImplItemType, ItemConst, ItemEnum, ItemExternCrate, ItemFn,
        ItemForeignMod, ItemImpl, ItemMacro, ItemMod, ItemStatic, ItemStruct, ItemTrait,
        ItemTraitAlias, ItemType, ItemUnion, ItemUse, Receiver, ReceiverKind, Safety, Signature,
        StaticMutability, TraitItemConst, TraitItemFn, TraitItemMacro, TraitItemType, UseGlob,
        UseGroup, UseName, UsePath, UseRename, Variadic, WhereClausePlacement,
    };
    use crate::mac::MacroDelimiter;
    use crate::path;
    use crate::path::printing::PathStyle;
    use crate::print::TokensOrDefault;
    use proc_macro2::TokenStream;
    use quote::{ToTokens, TokenStreamExt as _};

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ItemExternCrate {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.extern_token.to_tokens(tokens);
            self.crate_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            if let Some((as_token, rename)) = &self.rename {
                as_token.to_tokens(tokens);
                rename.to_tokens(tokens);
            }
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ItemUse {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.use_token.to_tokens(tokens);
            self.leading_colon.to_tokens(tokens);
            self.tree.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ItemStatic {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.static_token.to_tokens(tokens);
            self.mutability.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.colon_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
            self.eq_token.to_tokens(tokens);
            self.expr.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ItemConst {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.const_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.colon_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
            self.eq_token.to_tokens(tokens);
            self.expr.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ItemFn {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.sig.to_tokens(tokens);
            self.block.brace_token.surround(tokens, |tokens| {
                tokens.append_all(self.attrs.inner());
                tokens.append_all(&self.block.stmts);
            });
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ItemMod {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.unsafety.to_tokens(tokens);
            self.mod_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            if let Some((brace, items)) = &self.content {
                brace.surround(tokens, |tokens| {
                    tokens.append_all(self.attrs.inner());
                    tokens.append_all(items);
                });
            } else {
                TokensOrDefault(&self.semi).to_tokens(tokens);
            }
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ItemForeignMod {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.unsafety.to_tokens(tokens);
            self.abi.to_tokens(tokens);
            self.brace_token.surround(tokens, |tokens| {
                tokens.append_all(self.attrs.inner());
                tokens.append_all(&self.items);
            });
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ItemType {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.type_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            if let WhereClausePlacement::Early = self.where_clause_placement {
                self.generics.where_clause.to_tokens(tokens);
            }
            self.eq_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
            if let WhereClausePlacement::Late = self.where_clause_placement {
                self.generics.where_clause.to_tokens(tokens);
            }
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ItemEnum {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.enum_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            self.generics.where_clause.to_tokens(tokens);
            self.brace_token.surround(tokens, |tokens| {
                self.variants.to_tokens(tokens);
            });
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ItemStruct {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.struct_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            match &self.fields {
                Fields::Named(fields) => {
                    self.generics.where_clause.to_tokens(tokens);
                    fields.to_tokens(tokens);
                }
                Fields::Unnamed(fields) => {
                    fields.to_tokens(tokens);
                    self.generics.where_clause.to_tokens(tokens);
                    TokensOrDefault(&self.semi_token).to_tokens(tokens);
                }
                Fields::Unit => {
                    self.generics.where_clause.to_tokens(tokens);
                    TokensOrDefault(&self.semi_token).to_tokens(tokens);
                }
            }
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ItemUnion {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.union_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            self.generics.where_clause.to_tokens(tokens);
            self.fields.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ItemTrait {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.unsafety.to_tokens(tokens);
            self.modifiers.auto_token.to_tokens(tokens);
            self.trait_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            if !self.supertraits.is_empty() {
                TokensOrDefault(&self.colon_token).to_tokens(tokens);
                self.supertraits.to_tokens(tokens);
            }
            self.generics.where_clause.to_tokens(tokens);
            self.brace_token.surround(tokens, |tokens| {
                tokens.append_all(self.attrs.inner());
                tokens.append_all(&self.items);
            });
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ItemTraitAlias {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.trait_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            self.eq_token.to_tokens(tokens);
            self.bounds.to_tokens(tokens);
            self.generics.where_clause.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ItemImpl {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.modifiers.defaultness.to_tokens(tokens);
            self.unsafety.to_tokens(tokens);
            self.impl_token.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            self.modifiers.polarity.to_tokens(tokens);
            if let Some((path, for_token)) = &self.trait_ {
                path.to_tokens(tokens);
                for_token.to_tokens(tokens);
            }
            self.self_ty.to_tokens(tokens);
            self.generics.where_clause.to_tokens(tokens);
            self.brace_token.surround(tokens, |tokens| {
                tokens.append_all(self.attrs.inner());
                tokens.append_all(&self.items);
            });
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ItemMacro {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            path::printing::print_path(tokens, &self.mac.path, PathStyle::Mod);
            self.mac.bang_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            match &self.mac.delimiter {
                MacroDelimiter::Paren(paren) => {
                    paren.surround(tokens, |tokens| self.mac.tokens.to_tokens(tokens));
                }
                MacroDelimiter::Brace(brace) => {
                    brace.surround(tokens, |tokens| self.mac.tokens.to_tokens(tokens));
                }
                MacroDelimiter::Bracket(bracket) => {
                    bracket.surround(tokens, |tokens| self.mac.tokens.to_tokens(tokens));
                }
            }
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for UsePath {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.ident.to_tokens(tokens);
            self.colon2_token.to_tokens(tokens);
            self.tree.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for UseName {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.ident.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for UseRename {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.ident.to_tokens(tokens);
            self.as_token.to_tokens(tokens);
            self.rename.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for UseGlob {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.star_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for UseGroup {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.brace_token.surround(tokens, |tokens| {
                self.items.to_tokens(tokens);
            });
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TraitItemConst {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.const_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.colon_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
            if let Some((eq_token, default)) = &self.default {
                eq_token.to_tokens(tokens);
                default.to_tokens(tokens);
            }
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TraitItemFn {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.sig.to_tokens(tokens);
            match &self.default {
                Some(block) => {
                    block.brace_token.surround(tokens, |tokens| {
                        tokens.append_all(self.attrs.inner());
                        tokens.append_all(&block.stmts);
                    });
                }
                None => {
                    TokensOrDefault(&self.semi_token).to_tokens(tokens);
                }
            }
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TraitItemType {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.type_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            if !self.bounds.is_empty() {
                TokensOrDefault(&self.colon_token).to_tokens(tokens);
                self.bounds.to_tokens(tokens);
            }
            if let Some((eq_token, default)) = &self.default {
                eq_token.to_tokens(tokens);
                default.to_tokens(tokens);
            }
            self.generics.where_clause.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TraitItemMacro {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.mac.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ImplItemConst {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.modifiers.defaultness.to_tokens(tokens);
            self.const_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.colon_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
            self.eq_token.to_tokens(tokens);
            self.expr.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ImplItemFn {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.modifiers.defaultness.to_tokens(tokens);
            self.sig.to_tokens(tokens);
            self.block.brace_token.surround(tokens, |tokens| {
                tokens.append_all(self.attrs.inner());
                tokens.append_all(&self.block.stmts);
            });
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ImplItemType {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.modifiers.defaultness.to_tokens(tokens);
            self.type_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            self.eq_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
            self.generics.where_clause.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ImplItemMacro {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.mac.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ForeignItemFn {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.sig.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ForeignItemStatic {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.safety.to_tokens(tokens);
            self.static_token.to_tokens(tokens);
            self.mutability.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.colon_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ForeignItemType {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.type_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            self.generics.where_clause.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ForeignItemMacro {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            self.mac.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for Signature {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.constness.to_tokens(tokens);
            self.asyncness.to_tokens(tokens);
            self.safety.to_tokens(tokens);
            self.abi.to_tokens(tokens);
            self.fn_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            self.paren_token.surround(tokens, |tokens| {
                self.inputs.to_tokens(tokens);
                if let Some(variadic) = &self.variadic {
                    if !self.inputs.empty_or_trailing() {
                        <Token![,]>::default().to_tokens(tokens);
                    }
                    variadic.to_tokens(tokens);
                }
            });
            self.output.to_tokens(tokens);
            self.generics.where_clause.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for Safety {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            match self {
                Safety::Safe(token) => token.to_tokens(tokens),
                Safety::Unsafe(token) => token.to_tokens(tokens),
                Safety::Default => {}
            }
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for Receiver {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            match &self.kind {
                ReceiverKind::Value => {
                    self.mutability.to_tokens(tokens);
                    self.self_token.to_tokens(tokens);
                }
                ReceiverKind::Reference(ampersand, lifetime, mutability) => {
                    ampersand.to_tokens(tokens);
                    lifetime.to_tokens(tokens);
                    mutability.to_tokens(tokens);
                    self.self_token.to_tokens(tokens);
                }
                ReceiverKind::Typed(colon_token, ty) => {
                    self.mutability.to_tokens(tokens);
                    self.self_token.to_tokens(tokens);
                    colon_token.to_tokens(tokens);
                    ty.to_tokens(tokens);
                }
            }
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for Variadic {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            if let Some((pat, colon)) = &self.pat {
                pat.to_tokens(tokens);
                colon.to_tokens(tokens);
            }
            self.dots.to_tokens(tokens);
            self.comma.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for StaticMutability {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            match self {
                StaticMutability::None => {}
                StaticMutability::Mut(mut_token) => mut_token.to_tokens(tokens),
            }
        }
    }
}
