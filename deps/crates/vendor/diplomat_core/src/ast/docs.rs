use crate::ast::attrs::DiplomatBackendAttrCfg;

use super::Path;
use core::fmt;
use quote::ToTokens;
use serde::{Deserialize, Serialize};
use syn::parse::{self, Parse, ParseStream};
use syn::{Attribute, Ident, Meta, Token};

#[derive(Clone, PartialEq, Eq, Hash, Serialize, Deserialize, Debug)]
pub struct DocumentationSection {
    pub cfg: DiplomatBackendAttrCfg,
    pub lines: String,
}

impl Default for DocumentationSection {
    fn default() -> Self {
        Self {
            cfg: DiplomatBackendAttrCfg::Star,
            lines: String::default(),
        }
    }
}

#[derive(Clone, PartialEq, Eq, Hash, Serialize, Deserialize, Debug, Default)]
#[non_exhaustive]
pub struct Docs(pub Vec<DocumentationSection>, pub Vec<RustLink>);

impl Docs {
    pub fn from_attrs(attrs: &[Attribute]) -> Self {
        Self(Self::get_doc_lines(attrs), Self::get_rust_link(attrs))
    }

    fn get_doc_lines(attrs: &[Attribute]) -> Vec<DocumentationSection> {
        let mut sections: Vec<DocumentationSection> = vec![DocumentationSection::default()];
        let docs_path: syn::Path = syn::parse_str("diplomat::docs").unwrap();
        // Assume by default, that we are at #[diplomat::docs(*)]

        attrs.iter().for_each(|attr| {
            let active_section: &mut DocumentationSection = sections.last_mut().unwrap();

            if let Meta::NameValue(ref nv) = attr.meta {
                if nv.path.is_ident("doc") {
                    let node: syn::LitStr = syn::parse2(nv.value.to_token_stream()).unwrap();
                    let line = node.value().trim().to_string();

                    if !active_section.lines.is_empty() {
                        active_section.lines.push('\n');
                    }

                    active_section.lines.push_str(&line);
                }
            }
            if let Meta::List(ref l) = attr.meta {
                if l.path == docs_path {
                    let cfg = syn::parse2(l.tokens.clone()).unwrap();
                    sections.push(DocumentationSection {
                        lines: String::default(),
                        cfg,
                    });
                }
            }
        });

        sections
    }

    fn get_rust_link(attrs: &[Attribute]) -> Vec<RustLink> {
        attrs
            .iter()
            .filter(|i| i.path().to_token_stream().to_string() == "diplomat :: rust_link")
            .map(|i| i.parse_args().expect("Malformed attribute"))
            .collect()
    }

    pub fn rust_links(&self) -> &[RustLink] {
        &self.1
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
#[non_exhaustive]
pub enum RustLinkDisplay {
    /// A nice expanded representation that includes the type name
    ///
    /// e.g. "See the \[link to Rust documentation\] for more details"
    Normal,
    /// A compact representation that will fit multiple rust_link entries in one line
    ///
    /// E.g. "For further information, see: 1, 2, 3, 4" (all links)
    Compact,
    /// Hidden. Useful for programmatically annotating an API as related without showing a link to the user
    Hidden,
}

#[derive(Clone, PartialEq, Eq, Hash, Serialize, Deserialize, Debug, PartialOrd, Ord)]
#[non_exhaustive]
pub struct RustLink {
    pub path: Path,
    pub typ: DocType,
    pub display: RustLinkDisplay,
}

impl Parse for RustLink {
    fn parse(input: ParseStream<'_>) -> parse::Result<Self> {
        let path = input.parse()?;
        let path = Path::from_syn(&path);
        let _comma: Token![,] = input.parse()?;
        let ty_ident: Ident = input.parse()?;
        let typ = match &*ty_ident.to_string() {
            "Struct" => DocType::Struct,
            "StructField" => DocType::StructField,
            "Enum" => DocType::Enum,
            "EnumVariant" => DocType::EnumVariant,
            "EnumVariantField" => DocType::EnumVariantField,
            "Trait" => DocType::Trait,
            "FnInStruct" => DocType::FnInStruct,
            "FnInTypedef" => DocType::FnInTypedef,
            "FnInEnum" => DocType::FnInEnum,
            "FnInTrait" => DocType::FnInTrait,
            "DefaultFnInTrait" => DocType::DefaultFnInTrait,
            "Fn" => DocType::Fn,
            "Mod" => DocType::Mod,
            "Constant" => DocType::Constant,
            "AssociatedConstantInEnum" => DocType::AssociatedConstantInEnum,
            "AssociatedConstantInTrait" => DocType::AssociatedConstantInTrait,
            "AssociatedConstantInStruct" => DocType::AssociatedConstantInStruct,
            "Macro" => DocType::Macro,
            "AssociatedTypeInEnum" => DocType::AssociatedTypeInEnum,
            "AssociatedTypeInTrait" => DocType::AssociatedTypeInTrait,
            "AssociatedTypeInStruct" => DocType::AssociatedTypeInStruct,
            "Typedef" => DocType::Typedef,
            t => {
                return Err(parse::Error::new(
                    ty_ident.span(),
                    format!("Unknown rust_link doc type {t:?}"),
                ))
            }
        };
        let lookahead = input.lookahead1();
        let display = if lookahead.peek(Token![,]) {
            let _comma: Token![,] = input.parse()?;
            let display_ident: Ident = input.parse()?;
            match &*display_ident.to_string() {
                "normal" => RustLinkDisplay::Normal,
                "compact" => RustLinkDisplay::Compact,
                "hidden" => RustLinkDisplay::Hidden,
                _ => return Err(parse::Error::new(display_ident.span(), "Unknown rust_link display style: Must be must be `normal`, `compact`, or `hidden`.")),
            }
        } else {
            RustLinkDisplay::Normal
        };
        Ok(RustLink { path, typ, display })
    }
}
impl fmt::Display for RustLink {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}#{:?}", self.path, self.typ)
    }
}

#[derive(Clone, PartialEq, Eq, Hash, Serialize, Deserialize, Debug, PartialOrd, Ord)]
#[non_exhaustive]
pub enum DocType {
    Struct,
    StructField,
    Enum,
    EnumVariant,
    EnumVariantField,
    Trait,
    FnInStruct,
    FnInTypedef,
    FnInEnum,
    FnInTrait,
    DefaultFnInTrait,
    Fn,
    Mod,
    Constant,
    AssociatedConstantInEnum,
    AssociatedConstantInTrait,
    AssociatedConstantInStruct,
    Macro,
    AssociatedTypeInEnum,
    AssociatedTypeInTrait,
    AssociatedTypeInStruct,
    Typedef,
}
