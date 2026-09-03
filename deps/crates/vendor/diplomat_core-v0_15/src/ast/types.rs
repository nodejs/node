use proc_macro2::Span;
use quote::{ToTokens, TokenStreamExt};
use serde::{Deserialize, Serialize};
use syn::Token;

use std::fmt;
use std::ops::ControlFlow;
use std::str::FromStr;

use super::{
    Attrs, Docs, Enum, Ident, Lifetime, LifetimeEnv, LifetimeTransitivity, Method, NamedLifetime,
    OpaqueType, Path, RustLink, Struct, Trait,
};
use crate::{ast::Function, Env};

/// A type declared inside a Diplomat-annotated module.
#[derive(Clone, Serialize, Debug, Hash, PartialEq, Eq)]
#[non_exhaustive]
pub enum CustomType {
    /// A non-opaque struct whose fields will be visible across the FFI boundary.
    Struct(Struct),
    /// A type annotated with [`diplomat::opaque`] whose fields are not visible.
    Opaque(OpaqueType),
    /// A fieldless enum.
    Enum(Enum),
}

impl CustomType {
    /// Get the name of the custom type, which is unique within a module.
    pub fn name(&self) -> &Ident {
        match self {
            CustomType::Struct(strct) => &strct.name,
            CustomType::Opaque(strct) => &strct.name,
            CustomType::Enum(enm) => &enm.name,
        }
    }

    /// Get the methods declared in impls of the custom type.
    pub fn methods(&self) -> &Vec<Method> {
        match self {
            CustomType::Struct(strct) => &strct.methods,
            CustomType::Opaque(strct) => &strct.methods,
            CustomType::Enum(enm) => &enm.methods,
        }
    }

    pub fn attrs(&self) -> &Attrs {
        match self {
            CustomType::Struct(strct) => &strct.attrs,
            CustomType::Opaque(strct) => &strct.attrs,
            CustomType::Enum(enm) => &enm.attrs,
        }
    }

    /// Get the doc lines of the custom type.
    pub fn docs(&self) -> &Docs {
        match self {
            CustomType::Struct(strct) => &strct.docs,
            CustomType::Opaque(strct) => &strct.docs,
            CustomType::Enum(enm) => &enm.docs,
        }
    }

    /// Get all rust links on this type and its methods
    pub fn all_rust_links(&self) -> impl Iterator<Item = &RustLink> + '_ {
        [self.docs()]
            .into_iter()
            .chain(self.methods().iter().map(|m| m.docs()))
            .flat_map(|d| d.rust_links().iter())
    }

    pub fn self_path(&self, in_path: &Path) -> Path {
        in_path.sub_path(self.name().clone())
    }

    /// Get the lifetimes of the custom type.
    pub fn lifetimes(&self) -> Option<&LifetimeEnv> {
        match self {
            CustomType::Struct(strct) => Some(&strct.lifetimes),
            CustomType::Opaque(strct) => Some(&strct.lifetimes),
            CustomType::Enum(_) => None,
        }
    }
}

/// A symbol declared in a module, which can either be a pointer to another path,
/// or a custom type defined directly inside that module
#[derive(Clone, Serialize, Debug)]
#[non_exhaustive]
pub enum ModSymbol {
    /// A symbol that is a pointer to another path.
    Alias(Path),
    /// A symbol that is a submodule.
    SubModule(Ident),
    /// A symbol that is a custom type.
    CustomType(CustomType),
    /// A trait
    Trait(Trait),
    /// A free function not associated with any type.
    Function(Function),
}

/// A named type that is just a path, e.g. `std::borrow::Cow<'a, T>`.
#[derive(Clone, PartialEq, Eq, Hash, Serialize, Deserialize, Debug)]
#[non_exhaustive]
pub struct PathType {
    pub path: Path,
    pub lifetimes: Vec<Lifetime>,
}

impl PathType {
    pub fn to_syn(&self) -> syn::TypePath {
        let mut path = self.path.to_syn();

        if !self.lifetimes.is_empty() {
            if let Some(seg) = path.segments.last_mut() {
                let lifetimes = &self.lifetimes;
                seg.arguments =
                    syn::PathArguments::AngleBracketed(syn::parse_quote! { <#(#lifetimes),*> });
            }
        }

        syn::TypePath { qself: None, path }
    }

    pub fn new(path: Path) -> Self {
        Self {
            path,
            lifetimes: vec![],
        }
    }

    /// Get the `Self` type from a struct declaration.
    ///
    /// Consider the following struct declaration:
    /// ```
    /// struct RefList<'a> {
    ///     data: &'a i32,
    ///     next: Option<Box<Self>>,
    /// }
    /// ```
    /// When determining what type `Self` is in the `next` field, we would have to call
    /// this method on the `syn::ItemStruct` that represents this struct declaration.
    /// This method would then return a `PathType` representing `RefList<'a>`, so we
    /// know that's what `Self` should refer to.
    ///
    /// The reason this function exists though is so when we convert the fields' types
    /// to `PathType`s, we don't panic. We don't actually need to write the struct's
    /// field types expanded in the macro, so this function is more for correctness,
    pub fn extract_self_type(strct: &syn::ItemStruct) -> Self {
        let self_name = (&strct.ident).into();

        PathType {
            path: Path {
                elements: vec![self_name],
            },
            lifetimes: strct
                .generics
                .lifetimes()
                .map(|lt_def| (&lt_def.lifetime).into())
                .collect(),
        }
    }

    /// If this is a [`TypeName::Named`], grab the [`CustomType`] it points to from
    /// the `env`, which contains all [`CustomType`]s across all FFI modules.
    ///
    /// Also returns the path the CustomType is in (useful for resolving fields)
    pub fn resolve_with_path<'a>(&self, in_path: &Path, env: &'a Env) -> (Path, &'a CustomType) {
        let local_path = &self.path;
        let mut cur_path = in_path.clone();
        for (i, elem) in local_path.elements.iter().enumerate() {
            match elem.as_str() {
                "crate" => {
                    // TODO(#34): get the name of enclosing crate from env when we support multiple crates
                    cur_path = Path::empty()
                }

                "super" => cur_path = cur_path.get_super(),

                o => match env.get(&cur_path, o) {
                    Some(ModSymbol::Alias(p)) => {
                        let mut remaining_elements: Vec<Ident> =
                            local_path.elements.iter().skip(i + 1).cloned().collect();
                        let mut new_path = p.elements.clone();
                        new_path.append(&mut remaining_elements);
                        return PathType::new(Path { elements: new_path })
                            .resolve_with_path(&cur_path.clone(), env);
                    }
                    Some(ModSymbol::SubModule(name)) => {
                        cur_path.elements.push(name.clone());
                    }
                    Some(ModSymbol::CustomType(t)) => {
                        if i == local_path.elements.len() - 1 {
                            return (cur_path, t);
                        } else {
                            panic!(
                                "Unexpected custom type when resolving symbol {} in {}",
                                o,
                                cur_path.elements.join("::")
                            )
                        }
                    }
                    Some(ModSymbol::Trait(trt)) => {
                        panic!("Found trait {} but expected a type", trt.name);
                    }
                    Some(ModSymbol::Function(f)) => {
                        panic!("Found function {} but expected a type", f.name);
                    }
                    None => panic!(
                        "Could not resolve symbol {} in {}",
                        o,
                        cur_path.elements.join("::")
                    ),
                },
            }
        }

        panic!(
            "Path {} does not point to a custom type",
            in_path.elements.join("::")
        )
    }

    /// If this is a [`TypeName::Named`], grab the [`CustomType`] it points to from
    /// the `env`, which contains all [`CustomType`]s across all FFI modules.
    ///
    /// If you need to resolve struct fields later, call [`Self::resolve_with_path()`] instead
    /// to get the path to resolve the fields in.
    pub fn resolve<'a>(&self, in_path: &Path, env: &'a Env) -> &'a CustomType {
        self.resolve_with_path(in_path, env).1
    }

    pub fn trait_to_syn(&self) -> syn::TraitBound {
        let mut path = self.path.to_syn();

        if !self.lifetimes.is_empty() {
            if let Some(seg) = path.segments.last_mut() {
                let lifetimes = &self.lifetimes;
                seg.arguments =
                    syn::PathArguments::AngleBracketed(syn::parse_quote! { <#(#lifetimes),*> });
            }
        }
        syn::TraitBound {
            paren_token: None,
            modifier: syn::TraitBoundModifier::None,
            lifetimes: None, // todo this is an assumption
            path,
        }
    }

    pub fn resolve_trait_with_path<'a>(&self, in_path: &Path, env: &'a Env) -> (Path, Trait) {
        let local_path = &self.path;
        let cur_path = in_path.clone();
        for (i, elem) in local_path.elements.iter().enumerate() {
            if let Some(ModSymbol::Trait(trt)) = env.get(&cur_path, elem.as_str()) {
                if i == local_path.elements.len() - 1 {
                    return (cur_path, trt.clone());
                } else {
                    panic!(
                        "Unexpected custom trait when resolving symbol {} in {}",
                        trt.name,
                        cur_path.elements.join("::")
                    )
                }
            }
        }

        panic!(
            "Path {} does not point to a custom trait",
            in_path.elements.join("::")
        )
    }

    /// If this is a [`TypeName::Named`], grab the [`CustomType`] it points to from
    /// the `env`, which contains all [`CustomType`]s across all FFI modules.
    ///
    /// If you need to resolve struct fields later, call [`Self::resolve_with_path()`] instead
    /// to get the path to resolve the fields in.
    pub fn resolve_trait<'a>(&self, in_path: &Path, env: &'a Env) -> Trait {
        self.resolve_trait_with_path(in_path, env).1
    }
}

impl From<&syn::TypePath> for PathType {
    fn from(other: &syn::TypePath) -> Self {
        let lifetimes = other
            .path
            .segments
            .last()
            .and_then(|last| {
                if let syn::PathArguments::AngleBracketed(angle_generics) = &last.arguments {
                    Some(
                        angle_generics
                            .args
                            .iter()
                            .map(|generic_arg| match generic_arg {
                                syn::GenericArgument::Lifetime(lifetime) => lifetime.into(),
                                _ => panic!("generic type arguments are unsupported (type: {other:?}, arg: {generic_arg:?})"),
                            })
                            .collect(),
                    )
                } else {
                    None
                }
            })
            .unwrap_or_default();

        Self {
            path: Path::from_syn(&other.path),
            lifetimes,
        }
    }
}

impl From<&syn::TraitBound> for PathType {
    fn from(other: &syn::TraitBound) -> Self {
        let lifetimes = other
            .path
            .segments
            .last()
            .and_then(|last| {
                if let syn::PathArguments::AngleBracketed(angle_generics) = &last.arguments {
                    Some(
                        angle_generics
                            .args
                            .iter()
                            .map(|generic_arg| match generic_arg {
                                syn::GenericArgument::Lifetime(lifetime) => lifetime.into(),
                                _ => panic!("generic type arguments are unsupported {other:?}"),
                            })
                            .collect(),
                    )
                } else {
                    None
                }
            })
            .unwrap_or_default();

        Self {
            path: Path::from_syn(&other.path),
            lifetimes,
        }
    }
}

impl From<&syn::Signature> for PathType {
    fn from(other: &syn::Signature) -> Self {
        let lifetimes = other
            .generics
            .params
            .iter()
            .map(|generic_arg| match generic_arg {
                syn::GenericParam::Lifetime(lt) => (&lt.lifetime).into(),
                _ => panic!("generic type arguments are unsupported {other:?}"),
            })
            .collect();

        Self {
            path: Path::empty().sub_path((&other.ident).into()),
            lifetimes,
        }
    }
}

impl From<Path> for PathType {
    fn from(other: Path) -> Self {
        PathType::new(other)
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Serialize, Deserialize, Debug)]
#[allow(clippy::exhaustive_enums)] // there are only two kinds of mutability we care about
pub enum Mutability {
    Mutable,
    Immutable,
}

impl Mutability {
    pub fn to_syn(&self) -> Option<Token![mut]> {
        match self {
            Mutability::Mutable => Some(syn::token::Mut(Span::call_site())),
            Mutability::Immutable => None,
        }
    }

    pub fn from_syn(t: &Option<Token![mut]>) -> Self {
        match t {
            Some(_) => Mutability::Mutable,
            None => Mutability::Immutable,
        }
    }

    /// Returns `true` if `&self` is the mutable variant, otherwise `false`.
    pub fn is_mutable(&self) -> bool {
        matches!(self, Mutability::Mutable)
    }

    /// Returns `true` if `&self` is the immutable variant, otherwise `false`.
    pub fn is_immutable(&self) -> bool {
        matches!(self, Mutability::Immutable)
    }

    /// Shorthand ternary operator for choosing a value based on whether
    /// a `Mutability` is mutable or immutable.
    ///
    /// The following pattern (with very slight variations) shows up often in code gen:
    /// ```ignore
    /// if mutability.is_mutable() {
    ///     ""
    /// } else {
    ///     "const "
    /// }
    /// ```
    /// This is particularly annoying in `write!(...)` statements, where `cargo fmt`
    /// expands it to take up 5 lines.
    ///
    /// This method offers a 1-line alternative:
    /// ```ignore
    /// mutability.if_mut_else("", "const ")
    /// ```
    /// For cases where lazy evaluation is desired, consider using a conditional
    /// or a `match` statement.
    pub fn if_mut_else<T>(&self, if_mut: T, if_immut: T) -> T {
        match self {
            Mutability::Mutable => if_mut,
            Mutability::Immutable => if_immut,
        }
    }
}

/// For types like `Result`/`DiplomatResult`, `&[T]`/`DiplomatSlice<T>` which can be
/// specified using (non-ffi-safe) Rust stdlib types, or FFI-safe `repr(C)` types from
/// `diplomat_runtime`, this tracks which of the two were used.
#[derive(Copy, Clone, PartialEq, Eq, Hash, Serialize, Deserialize, Debug)]
#[allow(clippy::exhaustive_enums)] // This can only have two values
pub enum StdlibOrDiplomat {
    Stdlib,
    Diplomat,
}

/// A local type reference, such as the type of a field, parameter, or return value.
/// Unlike [`CustomType`], which represents a type declaration, [`TypeName`]s can compose
/// types through references and boxing, and can also capture unresolved paths.
#[derive(Clone, PartialEq, Eq, Hash, Serialize, Deserialize, Debug)]
#[non_exhaustive]
pub enum TypeName {
    /// A built-in Rust scalar primitive.
    Primitive(PrimitiveType),
    /// An unresolved path to a custom type, which can be resolved after all types
    /// are collected with [`TypeName::resolve()`].
    Named(PathType),
    /// An optionally mutable reference to another type.
    Reference(Lifetime, Mutability, Box<TypeName>),
    /// A `Box<T>` type.
    Box(Box<TypeName>),
    /// An `Option<T>` or DiplomatOption type.
    Option(Box<TypeName>, StdlibOrDiplomat),
    /// A `Result<T, E>` or `diplomat_runtime::DiplomatResult` type.
    Result(Box<TypeName>, Box<TypeName>, StdlibOrDiplomat),
    Write,
    /// A `&DiplomatStr` or `Box<DiplomatStr>` type.
    /// Owned strings don't have a lifetime.
    ///
    /// If StdlibOrDiplomat::Stdlib, it's specified using Rust pointer types (&T, Box<T>),
    /// if StdlibOrDiplomat::Diplomat, it's specified using DiplomatStrSlice, etc
    StrReference(Option<Lifetime>, StringEncoding, StdlibOrDiplomat),
    /// A `&[T]` or `Box<[T]>` type, where `T` is a primitive.
    /// Owned slices don't have a lifetime or mutability.
    ///
    /// If StdlibOrDiplomat::Stdlib, it's specified using Rust pointer types (&T, Box<T>),
    /// if StdlibOrDiplomat::Diplomat, it's specified using DiplomatSlice/DiplomatOwnedSlice/DiplomatSliceMut
    PrimitiveSlice(
        Option<(Lifetime, Mutability)>,
        PrimitiveType,
        StdlibOrDiplomat,
    ),
    /// `&[DiplomatStrSlice]`, etc. Equivalent to `&[&str]`
    ///
    /// If StdlibOrDiplomat::Stdlib, it's specified as `&[&DiplomatFoo]`, if StdlibOrDiplomat::Diplomat it's specified
    /// as `DiplomatSlice<&DiplomatFoo>`
    StrSlice(StringEncoding, StdlibOrDiplomat),
    /// `&[Struct]`. Meant for passing slices of structs where the struct's layout is known to be shared between Rust
    /// and the backend language. Primarily meant for large lists of compound types like `Vector3f64` or `Color4i16`.
    /// This is implemented on a per-backend basis.
    CustomTypeSlice(Option<(Lifetime, Mutability)>, Box<TypeName>),
    /// The `()` type.
    Unit,
    /// The `Self` type.
    SelfType(PathType),
    /// std::cmp::Ordering or core::cmp::Ordering
    ///
    /// The path must be present! Ordering will be parsed as an AST type!
    Ordering,
    Function(Vec<Box<TypeName>>, Box<TypeName>, Mutability),
    ImplTrait(PathType),
}

#[derive(Clone, PartialEq, Eq, Hash, Serialize, Deserialize, Debug, Copy)]
#[non_exhaustive]
pub enum StringEncoding {
    UnvalidatedUtf8,
    UnvalidatedUtf16,
    /// The caller guarantees that they're passing valid UTF-8, under penalty of UB
    Utf8,
}

impl StringEncoding {
    /// Get the diplomat slice type when specified using diplomat_runtime types
    pub fn get_diplomat_slice_type(self, lt: &Option<Lifetime>) -> syn::Type {
        if let Some(ref lt) = *lt {
            let lt = LifetimeGenericsListDisplay(lt);

            match self {
                Self::UnvalidatedUtf8 => {
                    syn::parse_quote_spanned!(Span::call_site() => diplomat_runtime::DiplomatStrSlice #lt)
                }
                Self::UnvalidatedUtf16 => {
                    syn::parse_quote_spanned!(Span::call_site() => diplomat_runtime::DiplomatStr16Slice #lt)
                }
                Self::Utf8 => {
                    syn::parse_quote_spanned!(Span::call_site() => diplomat_runtime::DiplomatUtf8StrSlice #lt)
                }
            }
        } else {
            match self {
                Self::UnvalidatedUtf8 => {
                    syn::parse_quote_spanned!(Span::call_site() => diplomat_runtime::DiplomatOwnedStrSlice)
                }
                Self::UnvalidatedUtf16 => {
                    syn::parse_quote_spanned!(Span::call_site() => diplomat_runtime::DiplomatOwnedStr16Slice)
                }
                Self::Utf8 => {
                    syn::parse_quote_spanned!(Span::call_site() => diplomat_runtime::DiplomatOwnedUTF8StrSlice)
                }
            }
        }
    }

    fn get_diplomat_slice_type_str(self) -> &'static str {
        match self {
            StringEncoding::Utf8 => "str",
            StringEncoding::UnvalidatedUtf8 => "DiplomatStr",
            StringEncoding::UnvalidatedUtf16 => "DiplomatStr16",
        }
    }
    /// Get slice type when specified using rust stdlib types
    pub fn get_stdlib_slice_type(self, lt: &Option<Lifetime>) -> syn::Type {
        let inner = match self {
            Self::UnvalidatedUtf8 => quote::quote!(DiplomatStr),
            Self::UnvalidatedUtf16 => quote::quote!(DiplomatStr16),
            Self::Utf8 => quote::quote!(str),
        };
        if let Some(ref lt) = *lt {
            let lt = ReferenceDisplay(lt, &Mutability::Immutable);

            syn::parse_quote_spanned!(Span::call_site() => #lt #inner)
        } else {
            syn::parse_quote_spanned!(Span::call_site() => Box<#inner>)
        }
    }
    pub fn get_stdlib_slice_type_str(self) -> &'static str {
        match self {
            StringEncoding::Utf8 => "DiplomatUtf8Str",
            StringEncoding::UnvalidatedUtf8 => "DiplomatStrSlice",
            StringEncoding::UnvalidatedUtf16 => "DiplomatStr16Slice",
        }
    }
}

fn get_lifetime_from_syn_path(p: &syn::TypePath) -> Lifetime {
    if let syn::PathArguments::AngleBracketed(ref generics) =
        p.path.segments[p.path.segments.len() - 1].arguments
    {
        if let Some(syn::GenericArgument::Lifetime(lt)) = generics.args.first() {
            return Lifetime::from(lt);
        }
    }
    Lifetime::Anonymous
}

fn get_ty_from_syn_path(p: &syn::TypePath) -> Option<&syn::Type> {
    if let syn::PathArguments::AngleBracketed(ref generics) =
        p.path.segments[p.path.segments.len() - 1].arguments
    {
        for gen in generics.args.iter() {
            if let syn::GenericArgument::Type(ref ty) = gen {
                return Some(ty);
            }
        }
    }
    None
}

impl TypeName {
    /// Is this type safe to be passed across the FFI boundary?
    ///
    /// This also marks DiplomatOption<&T> as FFI-unsafe: these are technically safe from an ABI standpoint
    /// however Diplomat always expects these to be equivalent to a nullable pointer, so Option<&T> is required.
    pub fn is_ffi_safe(&self) -> bool {
        match self {
            TypeName::Primitive(..) | TypeName::Named(_) | TypeName::SelfType(_) | TypeName::Reference(..) |
            TypeName::Box(..) |
            // can only be passed across the FFI boundary; callbacks and traits are input-only
            TypeName::Function(..) | TypeName::ImplTrait(..) |
            // These are specified using FFI-safe diplomat_runtime types
            TypeName::StrReference(.., StdlibOrDiplomat::Diplomat) | TypeName::StrSlice(.., StdlibOrDiplomat::Diplomat) |TypeName::PrimitiveSlice(.., StdlibOrDiplomat::Diplomat) | TypeName::CustomTypeSlice(..) => true,
            // These are special anyway and shouldn't show up in structs
            TypeName::Unit | TypeName::Write | TypeName::Result(..) |
            // This is basically only useful in return types
            TypeName::Ordering |
            // These are specified using Rust stdlib types and not safe across FFI
            TypeName::StrReference(.., StdlibOrDiplomat::Stdlib) | TypeName::StrSlice(.., StdlibOrDiplomat::Stdlib) | TypeName::PrimitiveSlice(.., StdlibOrDiplomat::Stdlib)  => false,
            TypeName::Option(inner, stdlib) => match **inner {
                // Option<&T>/Option<Box<T>> are the ffi-safe way to specify options
                TypeName::Reference(..) | TypeName::Box(..) => *stdlib == StdlibOrDiplomat::Stdlib,
                // For other types (primitives, structs, enums) we need DiplomatOption
                _ => *stdlib == StdlibOrDiplomat::Diplomat,
             }
        }
    }

    /// What's the FFI safe version of this type?
    ///
    /// This also marks DiplomatOption<&T> as FFI-unsafe: these are technically safe from an ABI standpoint
    /// however Diplomat always expects these to be equivalent to a nullable pointer, so Option<&T> is required.
    pub fn ffi_safe_version(&self) -> TypeName {
        match self {
            TypeName::StrReference(lt, encoding, StdlibOrDiplomat::Stdlib) => {
                TypeName::StrReference(lt.clone(), *encoding, StdlibOrDiplomat::Diplomat)
            }
            TypeName::StrSlice(encoding, StdlibOrDiplomat::Stdlib) => {
                TypeName::StrSlice(*encoding, StdlibOrDiplomat::Diplomat)
            }
            TypeName::PrimitiveSlice(ltmt, prim, StdlibOrDiplomat::Stdlib) => {
                TypeName::PrimitiveSlice(ltmt.clone(), *prim, StdlibOrDiplomat::Diplomat)
            }
            TypeName::Ordering => TypeName::Primitive(PrimitiveType::i8),
            TypeName::Option(inner, _stdlib) => match **inner {
                // Option<&T>/Option<Box<T>> are the ffi-safe way to specify options
                TypeName::Reference(..) | TypeName::Box(..) => {
                    TypeName::Option(inner.clone(), StdlibOrDiplomat::Stdlib)
                }
                // For other types (primitives, structs, enums) we need DiplomatOption
                _ => TypeName::Option(
                    Box::new(inner.ffi_safe_version()),
                    StdlibOrDiplomat::Diplomat,
                ),
            },
            TypeName::Result(ok, err, StdlibOrDiplomat::Stdlib) => {
                TypeName::Result(ok.clone(), err.clone(), StdlibOrDiplomat::Diplomat)
            }
            _ => self.clone(),
        }
    }
    /// Converts the [`TypeName`] back into an AST node that can be spliced into a program.
    pub fn to_syn(&self) -> syn::Type {
        match self {
            TypeName::Primitive(primitive) => {
                let primitive = primitive.to_ident();
                syn::parse_quote_spanned!(Span::call_site() => #primitive)
            }
            TypeName::Ordering => syn::parse_quote_spanned!(Span::call_site() => i8),
            TypeName::Named(name) | TypeName::SelfType(name) => {
                // Self also gets expanded instead of turning into `Self` because
                // this code is used to generate the `extern "C"` functions, which
                // aren't in an impl block.
                let name = name.to_syn();
                syn::parse_quote_spanned!(Span::call_site() => #name)
            }
            TypeName::Reference(lifetime, mutability, underlying) => {
                let reference = ReferenceDisplay(lifetime, mutability);
                let underlying = underlying.to_syn();

                syn::parse_quote_spanned!(Span::call_site() => #reference #underlying)
            }
            TypeName::Box(underlying) => {
                let underlying = underlying.to_syn();
                syn::parse_quote_spanned!(Span::call_site() => Box<#underlying>)
            }
            TypeName::Option(underlying, StdlibOrDiplomat::Stdlib) => {
                let underlying = underlying.to_syn();
                syn::parse_quote_spanned!(Span::call_site() => Option<#underlying>)
            }
            TypeName::Option(underlying, StdlibOrDiplomat::Diplomat) => {
                let underlying = underlying.to_syn();
                syn::parse_quote_spanned!(Span::call_site() => diplomat_runtime::DiplomatOption<#underlying>)
            }
            TypeName::Result(ok, err, StdlibOrDiplomat::Stdlib) => {
                let ok = ok.to_syn();
                let err = err.to_syn();
                syn::parse_quote_spanned!(Span::call_site() => Result<#ok, #err>)
            }
            TypeName::Result(ok, err, StdlibOrDiplomat::Diplomat) => {
                let ok = ok.to_syn();
                let err = err.to_syn();
                syn::parse_quote_spanned!(Span::call_site() => diplomat_runtime::DiplomatResult<#ok, #err>)
            }
            TypeName::Write => {
                syn::parse_quote_spanned!(Span::call_site() => diplomat_runtime::DiplomatWrite)
            }
            TypeName::StrReference(lt, encoding, is_stdlib_type) => {
                if *is_stdlib_type == StdlibOrDiplomat::Stdlib {
                    encoding.get_stdlib_slice_type(lt)
                } else {
                    encoding.get_diplomat_slice_type(lt)
                }
            }
            TypeName::StrSlice(encoding, is_stdlib_type) => {
                if *is_stdlib_type == StdlibOrDiplomat::Stdlib {
                    let inner = encoding.get_stdlib_slice_type(&Some(Lifetime::Anonymous));
                    syn::parse_quote_spanned!(Span::call_site() => &[#inner])
                } else {
                    let inner = encoding.get_diplomat_slice_type(&Some(Lifetime::Anonymous));
                    syn::parse_quote_spanned!(Span::call_site() => diplomat_runtime::DiplomatSlice<#inner>)
                }
            }
            TypeName::PrimitiveSlice(ltmt, primitive, is_stdlib_type) => {
                if *is_stdlib_type == StdlibOrDiplomat::Stdlib {
                    primitive.get_stdlib_slice_type(ltmt)
                } else {
                    primitive.get_diplomat_slice_type(ltmt)
                }
            }
            TypeName::CustomTypeSlice(ltmt, type_name) => {
                let inner = type_name.to_syn();
                if let Some((ref lt, ref mtbl)) = ltmt {
                    let reference = ReferenceDisplay(lt, mtbl);
                    syn::parse_quote_spanned!(Span::call_site() => #reference [#inner])
                } else {
                    syn::parse_quote_spanned! (Span::call_site() => &[#inner])
                }
            }

            TypeName::Unit => syn::parse_quote_spanned!(Span::call_site() => ()),
            TypeName::Function(_input_types, output_type, _mutability) => {
                // Convert the return type to something FFI-friendly:
                let output_type = output_type.ffi_safe_version().to_syn();
                // should be DiplomatCallback<function_output_type>
                syn::parse_quote_spanned!(Span::call_site() => DiplomatCallback<#output_type>)
            }
            TypeName::ImplTrait(trt_path) => {
                let trait_name =
                    Ident::from(format!("DiplomatTraitStruct_{}", trt_path.path.elements[0]));
                // should be DiplomatTraitStruct_trait_name
                syn::parse_quote_spanned!(Span::call_site() => #trait_name)
            }
        }
    }

    /// Extract a [`TypeName`] from a [`syn::Type`] AST node.
    /// The following rules are used to infer [`TypeName`] variants:
    /// - If the type is a path with a single element that is the name of a Rust primitive, returns a [`TypeName::Primitive`]
    /// - If the type is a path with a single element [`Box`], returns a [`TypeName::Box`] with the type parameter recursively converted
    /// - If the type is a path with a single element [`Option`], returns a [`TypeName::Option`] with the type parameter recursively converted
    /// - If the type is a path with a single element `Self` and `self_path_type` is provided, returns a [`TypeName::Named`]
    /// - If the type is a path with a single element [`Result`], returns a [`TypeName::Result`] with the type parameters recursively converted
    /// - If the type is a path equal to [`diplomat_runtime::DiplomatResult`], returns a [`TypeName::DiplomatResult`] with the type parameters recursively converted
    /// - If the type is a path equal to [`diplomat_runtime::DiplomatWrite`], returns a [`TypeName::Write`]
    /// - If the type is a owned or borrowed string type, returns a [`TypeName::StrReference`]
    /// - If the type is a owned or borrowed slice of a Rust primitive, returns a [`TypeName::PrimitiveSlice`]
    /// - If the type is a reference (`&` or `&mut`), returns a [`TypeName::Reference`] with the referenced type recursively converted
    /// - Otherwise, assume that the reference is to a [`CustomType`] in either the current module or another one, returns a [`TypeName::Named`]
    pub fn from_syn(ty: &syn::Type, self_path_type: Option<PathType>) -> TypeName {
        match ty {
            syn::Type::Reference(r) => {
                let lifetime = Lifetime::from(&r.lifetime);
                let mutability = Mutability::from_syn(&r.mutability);

                let name = r.elem.to_token_stream().to_string();
                if name.starts_with("DiplomatStr") || name == "str" {
                    if mutability.is_mutable() {
                        panic!("mutable string references are disallowed");
                    }
                    if name == "DiplomatStr" {
                        return TypeName::StrReference(
                            Some(lifetime),
                            StringEncoding::UnvalidatedUtf8,
                            StdlibOrDiplomat::Stdlib,
                        );
                    } else if name == "DiplomatStr16" {
                        return TypeName::StrReference(
                            Some(lifetime),
                            StringEncoding::UnvalidatedUtf16,
                            StdlibOrDiplomat::Stdlib,
                        );
                    } else if name == "str" {
                        return TypeName::StrReference(
                            Some(lifetime),
                            StringEncoding::Utf8,
                            StdlibOrDiplomat::Stdlib,
                        );
                    }
                }
                if let syn::Type::Slice(slice) = &*r.elem {
                    if let syn::Type::Path(p) = &*slice.elem {
                        if let Some(primitive) = p
                            .path
                            .get_ident()
                            .and_then(|i| PrimitiveType::from_str(i.to_string().as_str()).ok())
                        {
                            return TypeName::PrimitiveSlice(
                                Some((lifetime, mutability)),
                                primitive,
                                StdlibOrDiplomat::Stdlib,
                            );
                        }
                    }
                    if let TypeName::StrReference(
                        Some(Lifetime::Anonymous),
                        encoding,
                        is_stdlib_type,
                    ) = TypeName::from_syn(&slice.elem, self_path_type.clone())
                    {
                        if is_stdlib_type == StdlibOrDiplomat::Stdlib {
                            panic!("Slice-of-slice is only supported with DiplomatRuntime slice types (DiplomatStrSlice, DiplomatStr16Slice, DiplomatUtf8StrSlice)");
                        }
                        return TypeName::StrSlice(encoding, StdlibOrDiplomat::Stdlib);
                    }
                    return TypeName::CustomTypeSlice(
                        Some((lifetime, mutability)),
                        Box::new(TypeName::from_syn(slice.elem.as_ref(), self_path_type)),
                    );
                }
                TypeName::Reference(
                    lifetime,
                    mutability,
                    Box::new(TypeName::from_syn(r.elem.as_ref(), self_path_type)),
                )
            }
            syn::Type::Path(p) => {
                let p_len = p.path.segments.len();
                if let Some(primitive) = p
                    .path
                    .get_ident()
                    .and_then(|i| PrimitiveType::from_str(i.to_string().as_str()).ok())
                {
                    TypeName::Primitive(primitive)
                } else if p_len >= 2
                    && p.path.segments[p_len - 2].ident == "cmp"
                    && p.path.segments[p_len - 1].ident == "Ordering"
                {
                    TypeName::Ordering
                } else if p_len == 1 && p.path.segments[0].ident == "Box" {
                    if let syn::PathArguments::AngleBracketed(type_args) =
                        &p.path.segments[0].arguments
                    {
                        if let syn::GenericArgument::Type(syn::Type::Slice(slice)) =
                            &type_args.args[0]
                        {
                            if let TypeName::Primitive(p) =
                                TypeName::from_syn(&slice.elem, self_path_type)
                            {
                                TypeName::PrimitiveSlice(None, p, StdlibOrDiplomat::Stdlib)
                            } else {
                                panic!("Owned slices only support primitives.")
                            }
                        } else if let syn::GenericArgument::Type(tpe) = &type_args.args[0] {
                            if tpe.to_token_stream().to_string() == "DiplomatStr" {
                                TypeName::StrReference(
                                    None,
                                    StringEncoding::UnvalidatedUtf8,
                                    StdlibOrDiplomat::Stdlib,
                                )
                            } else if tpe.to_token_stream().to_string() == "DiplomatStr16" {
                                TypeName::StrReference(
                                    None,
                                    StringEncoding::UnvalidatedUtf16,
                                    StdlibOrDiplomat::Stdlib,
                                )
                            } else if tpe.to_token_stream().to_string() == "str" {
                                TypeName::StrReference(
                                    None,
                                    StringEncoding::Utf8,
                                    StdlibOrDiplomat::Stdlib,
                                )
                            } else {
                                TypeName::Box(Box::new(TypeName::from_syn(tpe, self_path_type)))
                            }
                        } else {
                            panic!("Expected first type argument for Box to be a type")
                        }
                    } else {
                        panic!("Expected angle brackets for Box type")
                    }
                } else if p_len == 1 && p.path.segments[0].ident == "Option"
                    || is_runtime_type(p, "DiplomatOption")
                {
                    if let syn::PathArguments::AngleBracketed(type_args) =
                        &p.path.segments[0].arguments
                    {
                        if let syn::GenericArgument::Type(tpe) = &type_args.args[0] {
                            let stdlib = if p.path.segments[0].ident == "Option" {
                                StdlibOrDiplomat::Stdlib
                            } else {
                                StdlibOrDiplomat::Diplomat
                            };
                            TypeName::Option(
                                Box::new(TypeName::from_syn(tpe, self_path_type)),
                                stdlib,
                            )
                        } else {
                            panic!("Expected first type argument for Option to be a type")
                        }
                    } else {
                        panic!("Expected angle brackets for Option type")
                    }
                } else if p_len == 1 && p.path.segments[0].ident == "Self" {
                    if let Some(self_path_type) = self_path_type {
                        TypeName::SelfType(self_path_type)
                    } else {
                        panic!("Cannot have `Self` type outside of a method");
                    }
                } else if is_runtime_type(p, "DiplomatOwnedStrSlice")
                    || is_runtime_type(p, "DiplomatOwnedStr16Slice")
                    || is_runtime_type(p, "DiplomatOwnedUTF8StrSlice")
                {
                    let encoding = if is_runtime_type(p, "DiplomatOwnedStrSlice") {
                        StringEncoding::UnvalidatedUtf8
                    } else if is_runtime_type(p, "DiplomatOwnedStr16Slice") {
                        StringEncoding::UnvalidatedUtf16
                    } else {
                        StringEncoding::Utf8
                    };

                    TypeName::StrReference(None, encoding, StdlibOrDiplomat::Diplomat)
                } else if is_runtime_type(p, "DiplomatStrSlice")
                    || is_runtime_type(p, "DiplomatStr16Slice")
                    || is_runtime_type(p, "DiplomatUtf8StrSlice")
                {
                    let lt = get_lifetime_from_syn_path(p);

                    let encoding = if is_runtime_type(p, "DiplomatStrSlice") {
                        StringEncoding::UnvalidatedUtf8
                    } else if is_runtime_type(p, "DiplomatStr16Slice") {
                        StringEncoding::UnvalidatedUtf16
                    } else {
                        StringEncoding::Utf8
                    };

                    TypeName::StrReference(Some(lt), encoding, StdlibOrDiplomat::Diplomat)
                } else if is_runtime_type(p, "DiplomatSlice")
                    || is_runtime_type(p, "DiplomatSliceMut")
                    || is_runtime_type(p, "DiplomatOwnedSlice")
                {
                    let ltmut = if is_runtime_type(p, "DiplomatOwnedSlice") {
                        None
                    } else {
                        let lt = get_lifetime_from_syn_path(p);
                        let mutability = if is_runtime_type(p, "DiplomatSlice") {
                            Mutability::Immutable
                        } else {
                            Mutability::Mutable
                        };
                        Some((lt, mutability))
                    };

                    let ty = get_ty_from_syn_path(p).expect("Expected type argument to DiplomatSlice/DiplomatSliceMut/DiplomatOwnedSlice");

                    if let syn::Type::Path(p) = &ty {
                        if let Some(ident) = p.path.get_ident() {
                            let ident = ident.to_string();
                            let i = ident.as_str();
                            match i {
                                "DiplomatStrSlice" => {
                                    return TypeName::StrSlice(
                                        StringEncoding::UnvalidatedUtf8,
                                        StdlibOrDiplomat::Diplomat,
                                    )
                                }
                                "DiplomatStr16Slice" => {
                                    return TypeName::StrSlice(
                                        StringEncoding::UnvalidatedUtf16,
                                        StdlibOrDiplomat::Diplomat,
                                    )
                                }
                                "DiplomatUtf8StrSlice" => {
                                    return TypeName::StrSlice(
                                        StringEncoding::Utf8,
                                        StdlibOrDiplomat::Diplomat,
                                    )
                                }
                                _ => {
                                    if let Ok(prim) = PrimitiveType::from_str(i) {
                                        return TypeName::PrimitiveSlice(
                                            ltmut,
                                            prim,
                                            StdlibOrDiplomat::Diplomat,
                                        );
                                    }
                                }
                            }
                        }
                    }
                    panic!("Found DiplomatSlice/DiplomatSliceMut/DiplomatOwnedSlice without primitive or DiplomatStrSlice-like generic");
                } else if p_len == 1 && p.path.segments[0].ident == "Result"
                    || is_runtime_type(p, "DiplomatResult")
                {
                    if let syn::PathArguments::AngleBracketed(type_args) =
                        &p.path.segments.last().unwrap().arguments
                    {
                        assert!(
                            type_args.args.len() > 1,
                            "Not enough arguments given to Result<T,E>. Are you using a non-std Result type?"
                        );

                        if let (syn::GenericArgument::Type(ok), syn::GenericArgument::Type(err)) =
                            (&type_args.args[0], &type_args.args[1])
                        {
                            let ok = TypeName::from_syn(ok, self_path_type.clone());
                            let err = TypeName::from_syn(err, self_path_type);
                            TypeName::Result(
                                Box::new(ok),
                                Box::new(err),
                                if is_runtime_type(p, "DiplomatResult") {
                                    StdlibOrDiplomat::Diplomat
                                } else {
                                    StdlibOrDiplomat::Stdlib
                                },
                            )
                        } else {
                            panic!("Expected both type arguments for Result to be a type")
                        }
                    } else {
                        panic!("Expected angle brackets for Result type")
                    }
                } else if is_runtime_type(p, "DiplomatWrite") {
                    TypeName::Write
                } else {
                    TypeName::Named(PathType::from(p))
                }
            }
            syn::Type::Tuple(tup) => {
                if tup.elems.is_empty() {
                    TypeName::Unit
                } else {
                    todo!("Tuples are not currently supported")
                }
            }
            syn::Type::ImplTrait(tr) => {
                let mut ret_type: Option<TypeName> = None;
                for trait_bound in &tr.bounds {
                    match trait_bound {
                        syn::TypeParamBound::Trait(syn::TraitBound { path: p, .. }) => {
                            if ret_type.is_some() {
                                todo!("Currently don't support implementing multiple traits");
                            }
                            let rel_segs = &p.segments;
                            let path_seg = &rel_segs[0];

                            // From the FFI side there's no real way to enforce the distinction between these two,
                            // but we can at least make the void* on the C-API side const or not.
                            let fn_mutability = match path_seg.ident.to_string().as_str() {
                                "Fn" => Some(Mutability::Immutable),
                                "FnMut" => Some(Mutability::Mutable),
                                _ => None,
                            };

                            if let Some(mutability) = fn_mutability {
                                // we're in a function type
                                // get input and output args
                                if let syn::PathArguments::Parenthesized(
                                    syn::ParenthesizedGenericArguments {
                                        inputs: input_types,
                                        output: output_type,
                                        ..
                                    },
                                ) = &path_seg.arguments
                                {
                                    // Validate none of the callback lifetimes are named - we only allow default behavior or 'static
                                    for input_type in input_types {
                                        if let syn::Type::Reference(syn::TypeReference {
                                            lifetime: Some(in_lifetime),
                                            ..
                                        }) = input_type
                                        {
                                            panic!("Lifetimes are not allowed on callback parameters: lifetime '{} on trait {} ", in_lifetime.ident, path_seg.ident);
                                        }
                                    }

                                    let in_types = input_types
                                        .iter()
                                        .map(|in_ty| {
                                            Box::new(TypeName::from_syn(
                                                in_ty,
                                                self_path_type.clone(),
                                            ))
                                        })
                                        .collect::<Vec<Box<TypeName>>>();

                                    let out_type = match output_type {
                                        syn::ReturnType::Type(_, output_type) => {
                                            TypeName::from_syn(output_type, self_path_type.clone())
                                        }
                                        syn::ReturnType::Default => TypeName::Unit,
                                    };

                                    // Validate lifetimes

                                    ret_type = Some(TypeName::Function(
                                        in_types,
                                        Box::new(out_type),
                                        mutability,
                                    ));
                                    continue;
                                }
                                panic!("Unsupported function type: {:?}", &path_seg.arguments);
                            } else {
                                ret_type =
                                    Some(TypeName::ImplTrait(PathType::from(&syn::TraitBound {
                                        paren_token: None,
                                        modifier: syn::TraitBoundModifier::None,
                                        lifetimes: None, // todo this is an assumption
                                        path: p.clone(),
                                    })));
                                continue;
                            }
                        }
                        syn::TypeParamBound::Lifetime(syn::Lifetime { ident, .. }) => {
                            assert_eq!(
                                ident, "static",
                                "only 'static lifetimes are supported on trait objects for now"
                            );
                        }
                        _ => {
                            panic!("Unsupported trait component: {trait_bound:?}");
                        }
                    }
                }
                ret_type.expect("No valid traits found")
            }
            other => panic!("Unsupported type: {}", other.to_token_stream()),
        }
    }

    /// Returns `true` if `self` is the `TypeName::SelfType` variant, otherwise
    /// `false`.
    pub fn is_self(&self) -> bool {
        matches!(self, TypeName::SelfType(_))
    }

    /// Recurse down the type tree, visiting all lifetimes.
    ///
    /// Using this function, you can collect all the lifetimes into a collection,
    /// or examine each one without having to make any additional allocations.
    pub fn visit_lifetimes<'a, F, B>(&'a self, visit: &mut F) -> ControlFlow<B>
    where
        F: FnMut(&'a Lifetime, LifetimeOrigin) -> ControlFlow<B>,
    {
        match self {
            TypeName::Named(path_type) | TypeName::SelfType(path_type) => path_type
                .lifetimes
                .iter()
                .try_for_each(|lt| visit(lt, LifetimeOrigin::Named)),
            TypeName::Reference(lt, _, ty) => {
                ty.visit_lifetimes(visit)?;
                visit(lt, LifetimeOrigin::Reference)
            }
            TypeName::Box(ty) | TypeName::Option(ty, _) => ty.visit_lifetimes(visit),
            TypeName::Result(ok, err, _) => {
                ok.visit_lifetimes(visit)?;
                err.visit_lifetimes(visit)
            }
            TypeName::StrReference(Some(lt), ..) => visit(lt, LifetimeOrigin::StrReference),
            TypeName::PrimitiveSlice(Some((lt, _)), ..) => {
                visit(lt, LifetimeOrigin::PrimitiveSlice)
            }
            _ => ControlFlow::Continue(()),
        }
    }

    /// Returns `true` if any lifetime satisfies a predicate, otherwise `false`.
    ///
    /// This method is short-circuiting, meaning that if the predicate ever succeeds,
    /// it will return immediately.
    pub fn any_lifetime<'a, F>(&'a self, mut f: F) -> bool
    where
        F: FnMut(&'a Lifetime, LifetimeOrigin) -> bool,
    {
        self.visit_lifetimes(&mut |lifetime, origin| {
            if f(lifetime, origin) {
                ControlFlow::Break(())
            } else {
                ControlFlow::Continue(())
            }
        })
        .is_break()
    }

    /// Returns `true` if all lifetimes satisfy a predicate, otherwise `false`.
    ///
    /// This method is short-circuiting, meaning that if the predicate ever fails,
    /// it will return immediately.
    pub fn all_lifetimes<'a, F>(&'a self, mut f: F) -> bool
    where
        F: FnMut(&'a Lifetime, LifetimeOrigin) -> bool,
    {
        self.visit_lifetimes(&mut |lifetime, origin| {
            if f(lifetime, origin) {
                ControlFlow::Continue(())
            } else {
                ControlFlow::Break(())
            }
        })
        .is_continue()
    }

    /// Returns all lifetimes in a [`LifetimeEnv`] that must live at least as
    /// long as the type.
    pub fn longer_lifetimes<'env>(
        &self,
        lifetime_env: &'env LifetimeEnv,
    ) -> Vec<&'env NamedLifetime> {
        self.transitive_lifetime_bounds(LifetimeTransitivity::longer(lifetime_env))
    }

    /// Returns all lifetimes in a [`LifetimeEnv`] that are outlived by the type.
    pub fn shorter_lifetimes<'env>(
        &self,
        lifetime_env: &'env LifetimeEnv,
    ) -> Vec<&'env NamedLifetime> {
        self.transitive_lifetime_bounds(LifetimeTransitivity::shorter(lifetime_env))
    }

    /// Visits the provided [`LifetimeTransitivity`] value with all `NamedLifetime`s
    /// in the type tree, and returns the transitively reachable lifetimes.
    fn transitive_lifetime_bounds<'env>(
        &self,
        mut transitivity: LifetimeTransitivity<'env>,
    ) -> Vec<&'env NamedLifetime> {
        // We don't use the control flow here
        let _ = self.visit_lifetimes(&mut |lifetime, _| -> ControlFlow<()> {
            if let Lifetime::Named(named) = lifetime {
                transitivity.visit(named);
            }
            ControlFlow::Continue(())
        });
        transitivity.finish()
    }

    pub fn is_zst(&self) -> bool {
        // check_zst() prevents non-unit types from being ZSTs
        matches!(*self, TypeName::Unit)
    }

    pub fn is_pointer(&self) -> bool {
        matches!(*self, TypeName::Reference(..) | TypeName::Box(_))
    }
}

#[non_exhaustive]
pub enum LifetimeOrigin {
    Named,
    Reference,
    StrReference,
    PrimitiveSlice,
}

fn is_runtime_type(p: &syn::TypePath, name: &str) -> bool {
    (p.path.segments.len() == 1 && p.path.segments[0].ident == name)
        || (p.path.segments.len() == 2
            && p.path.segments[0].ident == "diplomat_runtime"
            && p.path.segments[1].ident == name)
}

impl fmt::Display for TypeName {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            TypeName::Primitive(p) => p.fmt(f),
            TypeName::Ordering => write!(f, "Ordering"),
            TypeName::Named(p) | TypeName::SelfType(p) => p.fmt(f),
            TypeName::Reference(lifetime, mutability, typ) => {
                write!(f, "{}{typ}", ReferenceDisplay(lifetime, mutability))
            }
            TypeName::Box(typ) => write!(f, "Box<{typ}>"),
            TypeName::Option(typ, StdlibOrDiplomat::Stdlib) => write!(f, "Option<{typ}>"),
            TypeName::Option(typ, StdlibOrDiplomat::Diplomat) => write!(f, "DiplomatOption<{typ}>"),
            TypeName::Result(ok, err, _) => {
                write!(f, "Result<{ok}, {err}>")
            }
            TypeName::Write => "DiplomatWrite".fmt(f),
            TypeName::StrReference(lt, encoding, is_stdlib_type) => {
                if let Some(lt) = lt {
                    if *is_stdlib_type == StdlibOrDiplomat::Stdlib {
                        let lt = ReferenceDisplay(lt, &Mutability::Immutable);
                        let ty = encoding.get_diplomat_slice_type_str();
                        write!(f, "{lt}{ty}")
                    } else {
                        let ty = encoding.get_stdlib_slice_type_str();
                        let lt = LifetimeGenericsListDisplay(lt);
                        write!(f, "{ty}{lt}")
                    }
                } else {
                    match (encoding, is_stdlib_type) {
                        (_, StdlibOrDiplomat::Stdlib) => {
                            write!(f, "Box<{}>", encoding.get_diplomat_slice_type_str())
                        }
                        (StringEncoding::Utf8, StdlibOrDiplomat::Diplomat) => {
                            "DiplomatOwnedUtf8Str".fmt(f)
                        }
                        (StringEncoding::UnvalidatedUtf8, StdlibOrDiplomat::Diplomat) => {
                            "DiplomatOwnedStrSlice".fmt(f)
                        }
                        (StringEncoding::UnvalidatedUtf16, StdlibOrDiplomat::Diplomat) => {
                            "DiplomatOwnedStr16Slice".fmt(f)
                        }
                    }
                }
            }

            TypeName::StrSlice(encoding, StdlibOrDiplomat::Stdlib) => {
                let inner = encoding.get_stdlib_slice_type_str();

                write!(f, "&[&{inner}]")
            }
            TypeName::StrSlice(encoding, StdlibOrDiplomat::Diplomat) => {
                let inner = encoding.get_diplomat_slice_type_str();
                write!(f, "DiplomatSlice<{inner}>")
            }

            TypeName::PrimitiveSlice(
                Some((lifetime, mutability)),
                typ,
                StdlibOrDiplomat::Stdlib,
            ) => {
                write!(f, "{}[{typ}]", ReferenceDisplay(lifetime, mutability))
            }
            TypeName::PrimitiveSlice(
                Some((lifetime, mutability)),
                typ,
                StdlibOrDiplomat::Diplomat,
            ) => {
                let maybemut = if *mutability == Mutability::Immutable {
                    ""
                } else {
                    "Mut"
                };
                let lt = LifetimeGenericsListPartialDisplay(lifetime);
                write!(f, "DiplomatSlice{maybemut}<{lt}{typ}>")
            }
            TypeName::PrimitiveSlice(None, typ, _) => write!(f, "Box<[{typ}]>"),
            TypeName::CustomTypeSlice(Some((lifetime, mutability)), type_name) => {
                write!(f, "{}[{type_name}]", ReferenceDisplay(lifetime, mutability))
            }
            TypeName::CustomTypeSlice(None, type_name) => write!(f, "Box<[{type_name}]>"),
            TypeName::Unit => "()".fmt(f),
            TypeName::Function(input_types, out_type, _mutability) => {
                write!(f, "fn (")?;
                for in_typ in input_types.iter() {
                    write!(f, "{in_typ}")?;
                }
                write!(f, ")->{out_type}")
            }
            TypeName::ImplTrait(trt) => {
                write!(f, "impl ")?;
                trt.fmt(f)
            }
        }
    }
}

/// An [`fmt::Display`] type for formatting Rust references.
///
/// # Examples
///
/// ```ignore
/// let lifetime = Lifetime::from(&syn::parse_str::<syn::Lifetime>("'a"));
/// let mutability = Mutability::Mutable;
/// // ...
/// let fmt = format!("{}[u8]", ReferenceDisplay(&lifetime, &mutability));
///
/// assert_eq!(fmt, "&'a mut [u8]");
/// ```
struct ReferenceDisplay<'a>(&'a Lifetime, &'a Mutability);

impl<'a> fmt::Display for ReferenceDisplay<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self.0 {
            Lifetime::Static => "&'static ".fmt(f)?,
            Lifetime::Named(lifetime) => write!(f, "&{lifetime} ")?,
            Lifetime::Anonymous => '&'.fmt(f)?,
        }

        if self.1.is_mutable() {
            "mut ".fmt(f)?;
        }

        Ok(())
    }
}

impl<'a> quote::ToTokens for ReferenceDisplay<'a> {
    fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
        let lifetime = self.0.to_syn();
        let mutability = self.1.to_syn();

        tokens.append_all(quote::quote!(& #lifetime #mutability))
    }
}

/// An [`fmt::Display`] type for formatting Rust lifetimes as they show up in generics list, when
/// the generics list has no other elements
struct LifetimeGenericsListDisplay<'a>(&'a Lifetime);

impl<'a> fmt::Display for LifetimeGenericsListDisplay<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self.0 {
            Lifetime::Static => "<'static>".fmt(f),
            Lifetime::Named(lifetime) => write!(f, "<{lifetime}>"),
            Lifetime::Anonymous => Ok(()),
        }
    }
}

impl<'a> quote::ToTokens for LifetimeGenericsListDisplay<'a> {
    fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
        if let Lifetime::Anonymous = self.0 {
        } else {
            let lifetime = self.0.to_syn();
            tokens.append_all(quote::quote!(<#lifetime>))
        }
    }
}

/// An [`fmt::Display`] type for formatting Rust lifetimes as they show up in generics list, when
/// the generics list has another element
struct LifetimeGenericsListPartialDisplay<'a>(&'a Lifetime);

impl<'a> fmt::Display for LifetimeGenericsListPartialDisplay<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self.0 {
            Lifetime::Static => "'static,".fmt(f),
            Lifetime::Named(lifetime) => write!(f, "{lifetime},"),
            Lifetime::Anonymous => Ok(()),
        }
    }
}

impl<'a> quote::ToTokens for LifetimeGenericsListPartialDisplay<'a> {
    fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
        if let Lifetime::Anonymous = self.0 {
        } else {
            let lifetime = self.0.to_syn();
            tokens.append_all(quote::quote!(#lifetime,))
        }
    }
}

impl fmt::Display for PathType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.path.fmt(f)?;

        if let Some((first, rest)) = self.lifetimes.split_first() {
            write!(f, "<{first}")?;
            for lifetime in rest {
                write!(f, ", {lifetime}")?;
            }
            '>'.fmt(f)?;
        }
        Ok(())
    }
}

/// A built-in Rust primitive scalar type.
#[derive(Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize, Debug)]
#[allow(non_camel_case_types)]
#[allow(clippy::exhaustive_enums)] // there are only these (scalar types)
pub enum PrimitiveType {
    i8,
    u8,
    i16,
    u16,
    i32,
    u32,
    i64,
    u64,
    i128,
    u128,
    isize,
    usize,
    f32,
    f64,
    bool,
    char,
    /// a primitive byte that is not meant to be interpreted numerically
    /// in languages that don't have fine-grained integer types
    byte,
}

impl PrimitiveType {
    fn as_code_str(self) -> &'static str {
        match self {
            PrimitiveType::i8 => "i8",
            PrimitiveType::u8 => "u8",
            PrimitiveType::i16 => "i16",
            PrimitiveType::u16 => "u16",
            PrimitiveType::i32 => "i32",
            PrimitiveType::u32 => "u32",
            PrimitiveType::i64 => "i64",
            PrimitiveType::u64 => "u64",
            PrimitiveType::i128 => "i128",
            PrimitiveType::u128 => "u128",
            PrimitiveType::isize => "isize",
            PrimitiveType::usize => "usize",
            PrimitiveType::f32 => "f32",
            PrimitiveType::f64 => "f64",
            PrimitiveType::bool => "bool",
            PrimitiveType::char => "DiplomatChar",
            PrimitiveType::byte => "DiplomatByte",
        }
    }

    fn to_ident(self) -> proc_macro2::Ident {
        proc_macro2::Ident::new(self.as_code_str(), Span::call_site())
    }

    /// Get the type for a slice of this, as specified using Rust stdlib types
    pub fn get_stdlib_slice_type(self, lt: &Option<(Lifetime, Mutability)>) -> syn::Type {
        let primitive = self.to_ident();

        if let Some((ref lt, ref mtbl)) = lt {
            let reference = ReferenceDisplay(lt, mtbl);
            syn::parse_quote_spanned!(Span::call_site() => #reference [#primitive])
        } else {
            syn::parse_quote_spanned!(Span::call_site() => Box<[#primitive]>)
        }
    }

    /// Get the type for a slice of this, as specified using Diplomat runtime types
    pub fn get_diplomat_slice_type(self, lt: &Option<(Lifetime, Mutability)>) -> syn::Type {
        let primitive = self.to_ident();

        if let Some((lt, mtbl)) = lt {
            let lifetime = LifetimeGenericsListPartialDisplay(lt);

            if *mtbl == Mutability::Immutable {
                syn::parse_quote_spanned!(Span::call_site() => diplomat_runtime::DiplomatSlice<#lifetime #primitive>)
            } else {
                syn::parse_quote_spanned!(Span::call_site() => diplomat_runtime::DiplomatSliceMut<#lifetime #primitive>)
            }
        } else {
            syn::parse_quote_spanned!(Span::call_site() => diplomat_runtime::DiplomatOwnedSlice<#primitive>)
        }
    }
}

impl fmt::Display for PrimitiveType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            PrimitiveType::byte => "u8",
            PrimitiveType::char => "char",
            _ => self.as_code_str(),
        }
        .fmt(f)
    }
}

impl FromStr for PrimitiveType {
    type Err = ();
    fn from_str(s: &str) -> Result<Self, ()> {
        Ok(match s {
            "i8" => PrimitiveType::i8,
            "u8" => PrimitiveType::u8,
            "i16" => PrimitiveType::i16,
            "u16" => PrimitiveType::u16,
            "i32" => PrimitiveType::i32,
            "u32" => PrimitiveType::u32,
            "i64" => PrimitiveType::i64,
            "u64" => PrimitiveType::u64,
            "i128" => PrimitiveType::i128,
            "u128" => PrimitiveType::u128,
            "isize" => PrimitiveType::isize,
            "usize" => PrimitiveType::usize,
            "f32" => PrimitiveType::f32,
            "f64" => PrimitiveType::f64,
            "bool" => PrimitiveType::bool,
            "DiplomatChar" => PrimitiveType::char,
            "DiplomatByte" => PrimitiveType::byte,
            _ => return Err(()),
        })
    }
}

#[cfg(test)]
mod tests {
    use insta;

    use syn;

    use super::TypeName;

    #[test]
    fn typename_primitives() {
        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                i32
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                usize
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                bool
            },
            None
        ));
    }

    #[test]
    fn typename_named() {
        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                MyLocalStruct
            },
            None
        ));
    }

    #[test]
    fn typename_references() {
        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                &i32
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                &mut MyLocalStruct
            },
            None
        ));
    }

    #[test]
    fn typename_boxes() {
        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Box<i32>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Box<MyLocalStruct>
            },
            None
        ));
    }

    #[test]
    fn typename_option() {
        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Option<i32>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Option<MyLocalStruct>
            },
            None
        ));
    }

    #[test]
    fn typename_result() {
        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                DiplomatResult<MyLocalStruct, i32>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                DiplomatResult<(), MyLocalStruct>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Result<MyLocalStruct, i32>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Result<(), MyLocalStruct>
            },
            None
        ));
    }

    #[test]
    fn lifetimes() {
        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Foo<'a, 'b>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                ::core::my_type::Foo
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                ::core::my_type::Foo<'test>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Option<Ref<'object>>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Foo<'a, 'b, 'c, 'd>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                very::long::path::to::my::Type<'x, 'y, 'z>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                Result<OkRef<'a, 'b>, ErrRef<'c>>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                DiplomatSlice<'a, u16>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                DiplomatOwnedSlice<i8>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                DiplomatSliceStr<'a>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                DiplomatSliceMut<'a, f32>
            },
            None
        ));

        insta::assert_yaml_snapshot!(TypeName::from_syn(
            &syn::parse_quote! {
                DiplomatSlice<i32>
            },
            None
        ));
    }
}
