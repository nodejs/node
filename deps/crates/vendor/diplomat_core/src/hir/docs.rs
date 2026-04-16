use std::collections::HashMap;

use crate::{
    ast::{self, RustLink, RustLinkDisplay},
    hir::{AttributeValidator, ErrorStore},
};

#[derive(Debug, Clone)]
pub struct Docs {
    lines: String,
    rust_links: Vec<RustLink>,
}

#[non_exhaustive]
pub enum TypeReferenceSyntax {
    SquareBrackets,
    AtLink,
}

impl Docs {
    pub(crate) fn from_ast(
        d: &ast::Docs,
        validator: &dyn AttributeValidator,
        errors: &mut ErrorStore,
    ) -> Self {
        let mut out: Docs = Docs {
            lines: String::default(),
            rust_links: d.1.clone(),
        };

        for b in &d.0 {
            let satisfies = validator.satisfies_cfg(&b.cfg, None);
            if let Err(e) = satisfies {
                errors.push(e);
            } else if satisfies.unwrap() {
                if !out.lines.is_empty() {
                    out.lines.push('\n');
                }

                out.lines.push_str(&b.lines);
            }
        }

        out
    }

    /// Convert to markdown
    pub fn to_markdown(
        &self,
        ref_syntax: TypeReferenceSyntax,
        docs_url_gen: &DocsUrlGenerator,
    ) -> String {
        use std::fmt::Write;
        let mut lines = match ref_syntax {
            TypeReferenceSyntax::SquareBrackets => self.lines.replace("[`", "[").replace("`]", "]"),
            TypeReferenceSyntax::AtLink => self.lines.replace("[`", "{@link ").replace("`]", "}"),
        };

        let mut has_compact = false;
        for rust_link in &self.rust_links {
            if rust_link.display == RustLinkDisplay::Compact {
                has_compact = true;
            } else if rust_link.display == RustLinkDisplay::Normal {
                if !lines.is_empty() {
                    write!(lines, "\n\n").unwrap();
                }
                write!(
                    lines,
                    "See the [Rust documentation for `{name}`]({link}) for more information.",
                    name = rust_link.path.elements.last().unwrap(),
                    link = docs_url_gen.gen_for_rust_link(rust_link)
                )
                .unwrap();
            }
        }
        if has_compact {
            if !lines.is_empty() {
                write!(lines, "\n\n").unwrap();
            }
            write!(lines, "Additional information: ").unwrap();
            for (i, rust_link) in self
                .rust_links
                .iter()
                .filter(|r| r.display == RustLinkDisplay::Compact)
                .enumerate()
            {
                if i != 0 {
                    write!(lines, ", ").unwrap();
                }
                write!(
                    lines,
                    "[{}]({})",
                    i + 1,
                    docs_url_gen.gen_for_rust_link(rust_link)
                )
                .unwrap();
            }
        }
        lines
    }

    pub fn is_empty(&self) -> bool {
        self.lines.is_empty() && self.rust_links.is_empty()
    }
}

#[derive(Default)]
pub struct DocsUrlGenerator {
    default_url: Option<String>,
    base_urls: HashMap<String, String>,
}

impl DocsUrlGenerator {
    pub fn with_base_urls(default_url: Option<String>, base_urls: HashMap<String, String>) -> Self {
        Self {
            default_url,
            base_urls,
        }
    }

    fn gen_for_rust_link(&self, rust_link: &RustLink) -> String {
        use ast::DocType::*;

        let mut r = String::new();

        let base = self
            .base_urls
            .get(rust_link.path.elements[0].as_str())
            .map(String::as_str)
            .or(self.default_url.as_deref())
            .unwrap_or("https://docs.rs/");

        r.push_str(base);
        if !base.ends_with('/') {
            r.push('/');
        }
        if r == "https://docs.rs/" {
            r.push_str(rust_link.path.elements[0].as_str());
            r.push_str("/latest/");
        }

        let mut elements = rust_link.path.elements.iter().peekable();

        let module_depth = rust_link.path.elements.len()
            - match rust_link.typ {
                Mod => 0,
                Struct | Enum | Trait | Fn | Macro | Constant | Typedef => 1,
                FnInEnum
                | FnInStruct
                | FnInTypedef
                | FnInTrait
                | DefaultFnInTrait
                | EnumVariant
                | StructField
                | AssociatedTypeInEnum
                | AssociatedTypeInStruct
                | AssociatedTypeInTrait
                | AssociatedConstantInEnum
                | AssociatedConstantInStruct
                | AssociatedConstantInTrait => 2,
                EnumVariantField => 3,
            };

        for _ in 0..module_depth {
            r.push_str(elements.next().unwrap().as_str());
            r.push('/');
        }

        if elements.peek().is_none() {
            r.push_str("index.html");
            return r;
        }

        r.push_str(match rust_link.typ {
            Typedef | FnInTypedef => "type.",
            Struct
            | StructField
            | FnInStruct
            | AssociatedTypeInStruct
            | AssociatedConstantInStruct => "struct.",
            Enum
            | EnumVariant
            | EnumVariantField
            | FnInEnum
            | AssociatedTypeInEnum
            | AssociatedConstantInEnum => "enum.",
            Trait
            | FnInTrait
            | DefaultFnInTrait
            | AssociatedTypeInTrait
            | AssociatedConstantInTrait => "trait.",
            Fn => "fn.",
            Constant => "constant.",
            Macro => "macro.",
            Mod => unreachable!(),
        });

        r.push_str(elements.next().unwrap().as_str());

        r.push_str(".html");

        match rust_link.typ {
            FnInStruct | FnInEnum | DefaultFnInTrait | FnInTypedef => {
                r.push_str("#method.");
                r.push_str(elements.next().unwrap().as_str());
            }
            AssociatedTypeInStruct | AssociatedTypeInEnum | AssociatedTypeInTrait => {
                r.push_str("#associatedtype.");
                r.push_str(elements.next().unwrap().as_str());
            }
            AssociatedConstantInStruct | AssociatedConstantInEnum | AssociatedConstantInTrait => {
                r.push_str("#associatedconstant.");
                r.push_str(elements.next().unwrap().as_str());
            }
            FnInTrait => {
                r.push_str("#tymethod.");
                r.push_str(elements.next().unwrap().as_str());
            }
            EnumVariant => {
                r.push_str("#variant.");
                r.push_str(elements.next().unwrap().as_str());
            }
            StructField => {
                r.push_str("#structfield.");
                r.push_str(elements.next().unwrap().as_str());
            }
            EnumVariantField => {
                r.push_str("#variant.");
                r.push_str(elements.next().unwrap().as_str());
                r.push_str(".field.");
                r.push_str(elements.next().unwrap().as_str());
            }
            Struct | Enum | Trait | Fn | Mod | Constant | Macro | Typedef => {}
        }
        r
    }
}

#[test]
fn test_docs_url_generator() {
    use crate::hir::BasicAttributeValidator;
    let test_cases = [
        (
            syn::parse_quote! { #[diplomat::rust_link(std::foo::bar::batz, Struct)] },
            "https://docs.rs/std/latest/std/foo/bar/struct.batz.html",
        ),
        (
            syn::parse_quote! { #[diplomat::rust_link(std::foo::bar::batz, StructField)] },
            "https://docs.rs/std/latest/std/foo/struct.bar.html#structfield.batz",
        ),
        (
            syn::parse_quote! { #[diplomat::rust_link(std::foo::bar::batz, Enum)] },
            "https://docs.rs/std/latest/std/foo/bar/enum.batz.html",
        ),
        (
            syn::parse_quote! { #[diplomat::rust_link(std::foo::bar::batz, EnumVariant)] },
            "https://docs.rs/std/latest/std/foo/enum.bar.html#variant.batz",
        ),
        (
            syn::parse_quote! { #[diplomat::rust_link(std::foo::bar::batz, EnumVariantField)] },
            "https://docs.rs/std/latest/std/enum.foo.html#variant.bar.field.batz",
        ),
        (
            syn::parse_quote! { #[diplomat::rust_link(std::foo::bar::batz, Trait)] },
            "https://docs.rs/std/latest/std/foo/bar/trait.batz.html",
        ),
        (
            syn::parse_quote! { #[diplomat::rust_link(std::foo::bar::batz, FnInStruct)] },
            "https://docs.rs/std/latest/std/foo/struct.bar.html#method.batz",
        ),
        (
            syn::parse_quote! { #[diplomat::rust_link(std::foo::bar::batz, FnInEnum)] },
            "https://docs.rs/std/latest/std/foo/enum.bar.html#method.batz",
        ),
        (
            syn::parse_quote! { #[diplomat::rust_link(std::foo::bar::batz, FnInTrait)] },
            "https://docs.rs/std/latest/std/foo/trait.bar.html#tymethod.batz",
        ),
        (
            syn::parse_quote! { #[diplomat::rust_link(std::foo::bar::batz, DefaultFnInTrait)] },
            "https://docs.rs/std/latest/std/foo/trait.bar.html#method.batz",
        ),
        (
            syn::parse_quote! { #[diplomat::rust_link(std::foo::bar::batz, Fn)] },
            "https://docs.rs/std/latest/std/foo/bar/fn.batz.html",
        ),
        (
            syn::parse_quote! { #[diplomat::rust_link(std::foo::bar::batz, Mod)] },
            "https://docs.rs/std/latest/std/foo/bar/batz/index.html",
        ),
        (
            syn::parse_quote! { #[diplomat::rust_link(std::foo::bar::batz, Constant)] },
            "https://docs.rs/std/latest/std/foo/bar/constant.batz.html",
        ),
        (
            syn::parse_quote! { #[diplomat::rust_link(std::foo::bar::batz, Macro)] },
            "https://docs.rs/std/latest/std/foo/bar/macro.batz.html",
        ),
    ];

    for (attr, expected) in test_cases.clone() {
        assert_eq!(
            DocsUrlGenerator::default().gen_for_rust_link(
                &Docs::from_ast(
                    &ast::Docs::from_attrs(&[attr]),
                    &BasicAttributeValidator::default(),
                    &mut ErrorStore::default()
                )
                .rust_links[0]
            ),
            expected
        );
    }

    assert_eq!(
        DocsUrlGenerator::with_base_urls(
            None,
            [("std".to_string(), "http://std-docs.biz/".to_string())]
                .into_iter()
                .collect()
        )
        .gen_for_rust_link(
            &Docs::from_ast(
                &ast::Docs::from_attrs(&[test_cases[0].0.clone()]),
                &BasicAttributeValidator::default(),
                &mut ErrorStore::default()
            )
            .rust_links[0]
        ),
        "http://std-docs.biz/std/foo/bar/struct.batz.html"
    );

    assert_eq!(
        DocsUrlGenerator::with_base_urls(Some("http://std-docs.biz/".to_string()), HashMap::new())
            .gen_for_rust_link(
                &Docs::from_ast(
                    &ast::Docs::from_attrs(&[test_cases[0].0.clone()]),
                    &BasicAttributeValidator::default(),
                    &mut ErrorStore::default()
                )
                .rust_links[0]
            ),
        "http://std-docs.biz/std/foo/bar/struct.batz.html"
    );
}
