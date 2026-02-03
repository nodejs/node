//! This module contains utilities for dealing with Rust attributes

use serde::ser::{SerializeStruct, Serializer};
use serde::Serialize;
use std::borrow::Cow;
use std::convert::Infallible;
use std::str::FromStr;
use syn::parse::{Error as ParseError, Parse, ParseStream};
use syn::{Attribute, Expr, Ident, Lit, LitStr, Meta, MetaList, MetaNameValue, Token};

/// The list of attributes on a type. All attributes except `attrs` (HIR attrs) are
/// potentially read by the diplomat macro and the AST backends, anything that is not should
/// be added as an HIR attribute ([`crate::hir::Attrs`]).
///
/// # Inheritance
///
/// Attributes are typically "inherited": the attributes on a module
/// apply to all types and methods with it, the attributes on an impl apply to all
/// methods in it, and the attributes on an enum apply to all variants within it.
/// This allows the user to specify a single attribute to affect multiple fields.
///
/// However, the details of inheritance are not always the same for each attribute. For example, rename attributes
/// on a module only apply to the types within it (others methods would get doubly renamed).
///
/// Each attribute here documents its inheritance behavior. Note that the HIR attributes do not get inherited
/// during AST construction, since at that time it's unclear which of those attributes are actually available.
#[derive(Clone, PartialEq, Eq, Hash, Debug, Default)]
#[non_exhaustive]
pub struct Attrs {
    /// The regular #[cfg()] attributes. Inherited, though the inheritance onto methods is the
    /// only relevant one here.
    pub cfg: Vec<Attribute>,

    /// The #[deprecated(note = 'foo')] attribute.
    pub deprecated: Option<String>,

    /// HIR backend attributes.
    ///
    /// Inherited, but only during lowering. See [`crate::hir::Attrs`] for details on which HIR attributes are inherited.
    ///
    /// During AST attribute inheritance, HIR backend attributes are copied over from impls to their methods since the HIR does
    /// not see the impl blocks.
    pub attrs: Vec<DiplomatBackendAttr>,

    /// Renames to apply to the underlying C symbol. Can be found on methods, impls, and bridge modules, and is inherited.
    ///
    /// Affects method names when inherited onto methods.
    ///
    /// Affects destructor names when inherited onto types.
    ///
    /// Inherited.
    pub abi_rename: RenameAttr,

    /// For use by [`crate::hir::Attrs::demo_attrs`]
    pub demo_attrs: Vec<DemoBackendAttr>,
}

impl Attrs {
    fn add_attr(&mut self, attr: Attr) {
        match attr {
            Attr::Cfg(attr) => self.cfg.push(attr),
            Attr::DiplomatBackend(attr) => self.attrs.push(attr),
            Attr::CRename(rename) => self.abi_rename.extend(&rename),
            Attr::DemoBackend(attr) => self.demo_attrs.push(attr),
            Attr::Deprecated(msg) => self.deprecated = Some(msg),
        }
    }

    /// Get a copy of these attributes for use in inheritance, masking out things
    /// that should not be inherited
    pub(crate) fn attrs_for_inheritance(&self, context: AttrInheritContext) -> Self {
        // These attributes are inherited during lowering (since that's when they're parsed)
        //
        // Except for impls: lowering has no concept of impls so these get inherited early. This
        // is fine since impls have no inherent behavior and all attributes on impls are necessarily
        // only there for inheritance
        let attrs = if context == AttrInheritContext::MethodFromImpl {
            self.attrs.clone()
        } else {
            Vec::new()
        };

        let demo_attrs = if context == AttrInheritContext::MethodFromImpl {
            self.demo_attrs.clone()
        } else {
            Vec::new()
        };

        let abi_rename = self.abi_rename.attrs_for_inheritance(context, true);
        Self {
            cfg: self.cfg.clone(),

            deprecated: None,

            attrs,
            abi_rename,
            demo_attrs,
        }
    }

    pub(crate) fn add_attrs(&mut self, attrs: &[Attribute]) {
        for attr in syn_attr_to_ast_attr(attrs) {
            self.add_attr(attr)
        }
    }
    pub(crate) fn from_attrs(attrs: &[Attribute]) -> Self {
        let mut this = Self::default();
        this.add_attrs(attrs);
        this
    }
}

impl From<&[Attribute]> for Attrs {
    fn from(other: &[Attribute]) -> Self {
        Self::from_attrs(other)
    }
}

enum Attr {
    Cfg(Attribute),
    DiplomatBackend(DiplomatBackendAttr),
    CRename(RenameAttr),
    DemoBackend(DemoBackendAttr),
    Deprecated(String),
    // More goes here
}

fn syn_attr_to_ast_attr(attrs: &[Attribute]) -> impl Iterator<Item = Attr> + '_ {
    let cfg_path: syn::Path = syn::parse_str("cfg").unwrap();
    let dattr_path: syn::Path = syn::parse_str("diplomat::attr").unwrap();
    let crename_attr: syn::Path = syn::parse_str("diplomat::abi_rename").unwrap();
    let demo_path: syn::Path = syn::parse_str("diplomat::demo").unwrap();
    let deprecated: syn::Path = syn::parse_str("deprecated").unwrap();
    attrs.iter().filter_map(move |a| {
        if a.path() == &cfg_path {
            Some(Attr::Cfg(a.clone()))
        } else if a.path() == &dattr_path {
            Some(Attr::DiplomatBackend(
                a.parse_args()
                    .expect("Failed to parse malformed diplomat::attr"),
            ))
        } else if a.path() == &crename_attr {
            Some(Attr::CRename(RenameAttr::from_meta(&a.meta).unwrap()))
        } else if a.path() == &demo_path {
            Some(Attr::DemoBackend(
                a.parse_args()
                    .expect("Failed to parse malformed diplomat::demo"),
            ))
        } else if a.path() == &deprecated {
            if let Some(Meta::NameValue(MetaNameValue {
                value:
                    syn::Expr::Lit(syn::ExprLit {
                        lit: Lit::Str(s), ..
                    }),
                ..
            })) = a
                .meta
                .require_list()
                .ok()
                .and_then(|m| syn::parse2::<Meta>(m.tokens.clone()).ok())
            {
                Some(Attr::Deprecated(s.value()))
            } else {
                Some(Attr::Deprecated("deprecated".into()))
            }
        } else {
            None
        }
    })
}

impl Serialize for Attrs {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        // 1 is the number of fields in the struct.
        let mut state = serializer.serialize_struct("Attrs", 1)?;
        if !self.cfg.is_empty() {
            let cfg: Vec<_> = self
                .cfg
                .iter()
                .map(|a| quote::quote!(#a).to_string())
                .collect();
            state.serialize_field("cfg", &cfg)?;
        }
        if !self.attrs.is_empty() {
            state.serialize_field("attrs", &self.attrs)?;
        }
        if !self.abi_rename.is_empty() {
            state.serialize_field("abi_rename", &self.abi_rename)?;
        }
        state.end()
    }
}

/// A `#[diplomat::attr(...)]` attribute
///
/// Its contents must start with single element that is a CFG-expression
/// (so it may contain `foo = bar`, `foo = "bar"`, `ident`, `*` atoms,
/// and `all()`, `not()`, and `any()` combiners), and then be followed by one
/// or more backend-specific attributes, which can be any valid meta-item
#[derive(Clone, PartialEq, Eq, Hash, Debug, Serialize)]
#[non_exhaustive]
pub struct DiplomatBackendAttr {
    pub cfg: DiplomatBackendAttrCfg,
    #[serde(serialize_with = "serialize_meta")]
    pub meta: Meta,
}

fn serialize_meta<S>(m: &Meta, s: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    quote::quote!(#m).to_string().serialize(s)
}

#[derive(Clone, PartialEq, Eq, Hash, Debug, Serialize)]
#[non_exhaustive]
pub enum DiplomatBackendAttrCfg {
    Not(Box<DiplomatBackendAttrCfg>),
    Any(Vec<DiplomatBackendAttrCfg>),
    All(Vec<DiplomatBackendAttrCfg>),
    Star,
    // "auto", smartly figure out based on the attribute used
    Auto,
    BackendName(String),
    NameValue(String, String),
}

impl Parse for DiplomatBackendAttrCfg {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        let lookahead = input.lookahead1();
        if lookahead.peek(Ident) {
            let name: Ident = input.parse()?;
            if name == "auto" {
                Ok(DiplomatBackendAttrCfg::Auto)
            } else if name == "not" {
                let content;
                let _paren = syn::parenthesized!(content in input);
                Ok(DiplomatBackendAttrCfg::Not(Box::new(content.parse()?)))
            } else if name == "any" || name == "all" {
                let content;
                let _paren = syn::parenthesized!(content in input);
                let list = content.parse_terminated(Self::parse, Token![,])?;
                let vec = list.into_iter().collect();
                if name == "any" {
                    Ok(DiplomatBackendAttrCfg::Any(vec))
                } else {
                    Ok(DiplomatBackendAttrCfg::All(vec))
                }
            } else if input.peek(Token![=]) {
                let _t: Token![=] = input.parse()?;
                if input.peek(Ident) {
                    let value: Ident = input.parse()?;
                    Ok(DiplomatBackendAttrCfg::NameValue(
                        name.to_string(),
                        value.to_string(),
                    ))
                } else {
                    let value: LitStr = input.parse()?;
                    Ok(DiplomatBackendAttrCfg::NameValue(
                        name.to_string(),
                        value.value(),
                    ))
                }
            } else {
                Ok(DiplomatBackendAttrCfg::BackendName(name.to_string()))
            }
        } else if lookahead.peek(Token![*]) {
            let _t: Token![*] = input.parse()?;
            Ok(DiplomatBackendAttrCfg::Star)
        } else {
            Err(ParseError::new(
                input.span(),
                "CFG portion of #[diplomat::attr] fails to parse",
            ))
        }
    }
}

/// Meant to be used with Attribute::parse_args()
impl Parse for DiplomatBackendAttr {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        let cfg = input.parse()?;
        let _comma: Token![,] = input.parse()?;
        let meta = input.parse()?;
        Ok(Self { cfg, meta })
    }
}

// #region demo_gen specific attributes
/// A `#[diplomat::demo(...)]` attribute
#[derive(Clone, PartialEq, Eq, Hash, Debug, Serialize)]
#[non_exhaustive]
pub struct DemoBackendAttr {
    #[serde(serialize_with = "serialize_meta")]
    pub meta: Meta,
}

/// Meant to be used with Attribute::parse_args()
impl Parse for DemoBackendAttr {
    fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
        let meta = input.parse()?;
        Ok(Self { meta })
    }
}

// #endregion

#[derive(Copy, Clone, Eq, PartialEq, Ord, PartialOrd)]
pub(crate) enum AttrInheritContext {
    Variant,
    Type,
    /// When a method or an impl is inheriting from a module
    MethodOrImplFromModule,
    /// When a method is inheriting from an impl
    ///
    /// This distinction is made because HIR attributes are pre-inherited from the impl to the
    /// method, so the boundary of "method inheriting from module" is different
    MethodFromImpl,
    // Currently there's no way to feed an attribute to a Module, but such inheritance will
    // likely apply during lowering for config defaults.
    #[allow(unused)]
    Module,
}

/// A pattern for use in rename attributes, like `#[diplomat::abi_rename]`
///
/// This can be parsed from a string, typically something like `icu4x_{0}`.
/// It can have up to one {0} for replacement.
///
/// In the future this may support transformations like to_camel_case, etc,
/// probably specified as a list like `#[diplomat::abi_rename("foo{0}", to_camel_case)]`
#[derive(Default, Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Debug, Serialize)]
pub struct RenameAttr {
    pattern: Option<RenamePattern>,
}

impl RenameAttr {
    /// Apply all renames to a given string
    pub fn apply<'a>(&'a self, name: Cow<'a, str>) -> Cow<'a, str> {
        if let Some(ref pattern) = self.pattern {
            let replacement = &pattern.replacement;
            if let Some(index) = pattern.insertion_index {
                format!("{}{name}{}", &replacement[..index], &replacement[index..]).into()
            } else {
                replacement.into()
            }
        } else {
            name
        }
    }

    /// Whether this rename is empty and will perform no changes
    pub(crate) fn is_empty(&self) -> bool {
        self.pattern.is_none()
    }

    pub(crate) fn extend(&mut self, other: &Self) {
        if other.pattern.is_some() {
            self.pattern.clone_from(&other.pattern);
        }

        // In the future if we support things like to_lower_case they may inherit separately
        // from patterns.
    }

    /// Get a copy of these attributes for use in inheritance, masking out things
    /// that should not be inherited
    pub(crate) fn attrs_for_inheritance(
        &self,
        context: AttrInheritContext,
        is_abi_rename: bool,
    ) -> Self {
        let pattern = match context {
            // No inheritance from modules to method-likes for the rename attribute
            AttrInheritContext::MethodOrImplFromModule if !is_abi_rename => Default::default(),
            // No effect on variants
            AttrInheritContext::Variant => Default::default(),
            _ => self.pattern.clone(),
        };
        // In the future if we support things like to_lower_case they may inherit separately
        // from patterns.
        Self { pattern }
    }

    /// From a replacement pattern, like "icu4x_{0}". Can have up to one {0} in it for substitution.
    fn from_pattern(s: &str) -> Self {
        Self {
            pattern: Some(s.parse().unwrap()),
        }
    }

    pub(crate) fn from_meta(meta: &Meta) -> Result<Self, &'static str> {
        let attr = StandardAttribute::from_meta(meta)
            .map_err(|_| "#[diplomat::abi_rename] must be given a string value")?;

        match attr {
            StandardAttribute::String(s) => Ok(RenameAttr::from_pattern(&s)),
            StandardAttribute::List(_) => {
                Err("Failed to parse malformed #[diplomat::abi_rename(...)]: found list")
            }
            StandardAttribute::Empty => {
                Err("Failed to parse malformed #[diplomat::abi_rename(...)]: found no parameters")
            }
        }
    }
}

impl FromStr for RenamePattern {
    type Err = Infallible;
    fn from_str(s: &str) -> Result<Self, Infallible> {
        if let Some(index) = s.find("{0}") {
            let replacement = format!("{}{}", &s[..index], &s[index + 3..]);
            Ok(Self {
                replacement,
                insertion_index: Some(index),
            })
        } else {
            Ok(Self {
                replacement: s.into(),
                insertion_index: None,
            })
        }
    }
}

#[derive(Clone, Eq, PartialEq, Ord, PartialOrd, Hash, Debug, Serialize)]
struct RenamePattern {
    /// The string to replace with
    replacement: String,
    /// The index in `replacement` in which to insert the original string. If None,
    /// this is a pure rename
    insertion_index: Option<usize>,
}

/// Helper type for parsing standard attributes. A standard attribute typically will accept the forms:
///
/// - `#[attr = "foo"]` and `#[attr("foo")]` for a simple string
/// - `#[attr(....)]` for a more complicated context
/// - `#[attr]` for a "defaulting" context
///
/// This allows attributes to parse simple string values without caring too much about the NameValue vs List representation
/// and then attributes can choose to handle more complicated lists if they so desire.
pub(crate) enum StandardAttribute<'a> {
    String(String),
    List(#[allow(dead_code)] &'a MetaList),
    Empty,
}

impl<'a> StandardAttribute<'a> {
    /// Parse from a Meta. Returns an error when no string value is specified in the path/namevalue forms.
    pub(crate) fn from_meta(meta: &'a Meta) -> Result<Self, ()> {
        match meta {
            Meta::Path(..) => Ok(Self::Empty),
            Meta::NameValue(ref nv) => {
                // Support a shortcut `abi_rename = "..."`
                let Expr::Lit(ref lit) = nv.value else {
                    return Err(());
                };
                let Lit::Str(ref lit) = lit.lit else {
                    return Err(());
                };
                Ok(Self::String(lit.value()))
            }
            // The full syntax to which we'll add more things in the future, `abi_rename("")`
            Meta::List(list) => {
                if let Ok(lit) = list.parse_args::<LitStr>() {
                    Ok(Self::String(lit.value()))
                } else {
                    Ok(Self::List(list))
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use insta;

    use syn;

    use super::{DiplomatBackendAttr, DiplomatBackendAttrCfg, RenameAttr};

    #[test]
    fn test_cfgs() {
        let attr_cfg: DiplomatBackendAttrCfg = syn::parse_quote!(*);
        insta::assert_yaml_snapshot!(attr_cfg);
        let attr_cfg: DiplomatBackendAttrCfg = syn::parse_quote!(cpp);
        insta::assert_yaml_snapshot!(attr_cfg);
        let attr_cfg: DiplomatBackendAttrCfg = syn::parse_quote!(has = overloading);
        insta::assert_yaml_snapshot!(attr_cfg);
        let attr_cfg: DiplomatBackendAttrCfg = syn::parse_quote!(has = "overloading");
        insta::assert_yaml_snapshot!(attr_cfg);
        let attr_cfg: DiplomatBackendAttrCfg =
            syn::parse_quote!(any(all(*, cpp, has="overloading"), not(js)));
        insta::assert_yaml_snapshot!(attr_cfg);
    }

    #[test]
    fn test_attr() {
        let attr: syn::Attribute =
            syn::parse_quote!(#[diplomat::attr(any(cpp, has = "overloading"), namespacing)]);
        let attr: DiplomatBackendAttr = attr.parse_args().unwrap();
        insta::assert_yaml_snapshot!(attr);
    }

    #[test]
    fn test_rename() {
        let attr: syn::Attribute = syn::parse_quote!(#[diplomat::abi_rename = "foobar_{0}"]);
        let attr = RenameAttr::from_meta(&attr.meta).unwrap();
        insta::assert_yaml_snapshot!(attr);
        let attr: syn::Attribute = syn::parse_quote!(#[diplomat::abi_rename("foobar_{0}")]);
        let attr = RenameAttr::from_meta(&attr.meta).unwrap();
        insta::assert_yaml_snapshot!(attr);
    }
}
