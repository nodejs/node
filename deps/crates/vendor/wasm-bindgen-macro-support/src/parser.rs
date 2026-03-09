use std::cell::{Cell, RefCell};
use std::collections::HashMap;
use std::str::Chars;
use std::{char, iter};

use ast::OperationKind;
use proc_macro2::{Ident, Span, TokenStream, TokenTree};
use quote::ToTokens;
use syn::ext::IdentExt;
use syn::parse::{Parse, ParseStream, Result as SynResult};
use syn::spanned::Spanned;
use syn::visit_mut::VisitMut;
use syn::Token;
use syn::{ItemFn, Lit, MacroDelimiter, ReturnType};
use wasm_bindgen_shared::identifier::{is_js_keyword, is_non_value_js_keyword, is_valid_ident};

use crate::ast::{self, ThreadLocal};
use crate::hash::ShortHash;
use crate::ClassMarker;
use crate::Diagnostic;

thread_local!(static ATTRS: AttributeParseState = Default::default());

/// Return an [`Err`] if the given string contains a comment close syntax (`*/``).
fn check_js_comment_close(str: &str, span: Span) -> Result<(), Diagnostic> {
    if str.contains("*/") {
        Err(Diagnostic::span_error(
            span,
            "contains comment close syntax",
        ))
    } else {
        Ok(())
    }
}

/// Return an [`Err`] if the given string is a JS keyword or contains a comment close syntax (`*/``).
fn check_invalid_type(str: &str, span: Span) -> Result<(), Diagnostic> {
    if is_js_keyword(str) {
        return Err(Diagnostic::span_error(span, "collides with JS keyword"));
    }
    check_js_comment_close(str, span)?;
    Ok(())
}

#[derive(Default)]
struct AttributeParseState {
    parsed: Cell<usize>,
    checks: Cell<usize>,
    unused_attrs: RefCell<Vec<UnusedState>>,
}

struct UnusedState {
    error: bool,
    ident: Ident,
}

/// Parsed attributes from a `#[wasm_bindgen(..)]`.
#[cfg_attr(feature = "extra-traits", derive(Debug))]
pub struct BindgenAttrs {
    /// List of parsed attributes
    pub attrs: Vec<(Cell<bool>, BindgenAttr)>,
}

/// A list of identifiers representing the namespace prefix of an imported
/// function or constant, or for exported types.
///
/// The list is guaranteed to be non-empty and not start with a non-value JS keyword
/// (except for "default", which is allowed as a special case).
#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Clone)]
pub struct JsNamespace(Vec<String>);

macro_rules! attrgen {
    ($mac:ident) => {
        $mac! {
            (catch, false, Catch(Span)),
            (constructor, false, Constructor(Span)),
            (method, false, Method(Span)),
            (r#this, false, This(Span)),
            (static_method_of, false, StaticMethodOf(Span, Ident)),
            (js_namespace, false, JsNamespace(Span, JsNamespace, Vec<Span>)),
            (module, true, Module(Span, String, Span)),
            (raw_module, true, RawModule(Span, String, Span)),
            (inline_js, true, InlineJs(Span, String, Span)),
            (getter, false, Getter(Span, Option<String>)),
            (setter, false, Setter(Span, Option<String>)),
            (indexing_getter, false, IndexingGetter(Span)),
            (indexing_setter, false, IndexingSetter(Span)),
            (indexing_deleter, false, IndexingDeleter(Span)),
            (structural, false, Structural(Span)),
            (r#final, false, Final(Span)),
            (readonly, false, Readonly(Span)),
            (js_name, false, JsName(Span, String, Span)),
            (js_class, false, JsClass(Span, String, Span)),
            (reexport, false, Reexport(Span, Option<String>)),
            (inspectable, false, Inspectable(Span)),
            (is_type_of, false, IsTypeOf(Span, syn::Expr)),
            (extends, false, Extends(Span, syn::Path)),
            (no_deref, false, NoDeref(Span)),
            (no_upcast, false, NoUpcast(Span)),
            (no_promising, false, NoPromising(Span)),
            (vendor_prefix, false, VendorPrefix(Span, Ident)),
            (variadic, false, Variadic(Span)),
            (typescript_custom_section, false, TypescriptCustomSection(Span)),
            (skip_typescript, false, SkipTypescript(Span)),
            (skip_jsdoc, false, SkipJsDoc(Span)),
            (private, false, Hide(Span)),
            (main, false, Main(Span)),
            (start, false, Start(Span)),
            (wasm_bindgen, false, WasmBindgen(Span, syn::Path)),
            (js_sys, false, JsSys(Span, syn::Path)),
            (wasm_bindgen_futures, false, WasmBindgenFutures(Span, syn::Path)),
            (skip, false, Skip(Span)),
            (typescript_type, false, TypeScriptType(Span, String, Span)),
            (getter_with_clone, false, GetterWithClone(Span)),
            (static_string, false, StaticString(Span)),
            (thread_local, false, ThreadLocal(Span)),
            (thread_local_v2, false, ThreadLocalV2(Span)),
            (unchecked_return_type, true, ReturnType(Span, String, Span)),
            (return_description, true, ReturnDesc(Span, String, Span)),
            (unchecked_param_type, true, ParamType(Span, String, Span)),
            (unchecked_optional_param_type, true, OptionalParamType(Span, String, Span)),
            (param_description, true, ParamDesc(Span, String, Span)),

            // For testing purposes only.
            (assert_no_shim, false, AssertNoShim(Span)),
        }
    };
}

macro_rules! methods {
    ($(($name:ident, $invalid_unused:literal, $variant:ident($($contents:tt)*)),)*) => {
        $(methods!(@method $name, $variant($($contents)*));)*

        fn enforce_used(self) -> Result<(), Diagnostic> {
            // Account for the fact this method was called
            ATTRS.with(|state| state.checks.set(state.checks.get() + 1));

            let mut errors = Vec::new();
            for (used, attr) in self.attrs.iter() {
                if used.get() {
                    continue
                }
                let span = match attr {
                    $(BindgenAttr::$variant(span, ..) => span,)*
                };
                errors.push(Diagnostic::span_error(*span, "unused wasm_bindgen attribute"));
            }
            Diagnostic::from_vec(errors)
        }

        fn check_used(self) {
            // Account for the fact this method was called
            ATTRS.with(|state| {
                state.checks.set(state.checks.get() + 1);

                state.unused_attrs.borrow_mut().extend(
                    self.attrs
                    .iter()
                    .filter_map(|(used, attr)| if used.get() { None } else { Some(attr) })
                    .map(|attr| {
                        match attr {
                            $(BindgenAttr::$variant(span, ..) => {
                                UnusedState {
                                    error: $invalid_unused,
                                    ident: syn::parse_quote_spanned!(*span => $name)
                                }
                            })*
                        }
                    })
                );
            });
        }
    };

    (@method $name:ident, $variant:ident(Span, String, Span)) => {
        pub(crate) fn $name(&self) -> Option<(&str, Span)> {
            self.attrs
                .iter()
                .find_map(|a| match &a.1 {
                    BindgenAttr::$variant(_, s, span) => {
                        a.0.set(true);
                        Some((&s[..], *span))
                    }
                    _ => None,
                })
        }
    };

    (@method $name:ident, $variant:ident(Span, JsNamespace, Vec<Span>)) => {
        pub(crate) fn $name(&self) -> Option<(JsNamespace, &[Span])> {
            self.attrs
                .iter()
                .find_map(|a| match &a.1 {
                    BindgenAttr::$variant(_, ss, spans) => {
                        a.0.set(true);
                        Some((ss.clone(), &spans[..]))
                    }
                    _ => None,
                })
        }
    };

    (@method $name:ident, $variant:ident(Span, $($other:tt)*)) => {
        #[allow(unused)]
        pub(crate) fn $name(&self) -> Option<&$($other)*> {
            self.attrs
                .iter()
                .find_map(|a| match &a.1 {
                    BindgenAttr::$variant(_, s) => {
                        a.0.set(true);
                        Some(s)
                    }
                    _ => None,
                })
        }
    };

    (@method $name:ident, $variant:ident($($other:tt)*)) => {
        #[allow(unused)]
        pub(crate) fn $name(&self) -> Option<&$($other)*> {
            self.attrs
                .iter()
                .find_map(|a| match &a.1 {
                    BindgenAttr::$variant(s) => {
                        a.0.set(true);
                        Some(s)
                    }
                    _ => None,
                })
        }
    };
}

impl BindgenAttrs {
    /// Find and parse the wasm_bindgen attributes.
    fn find(attrs: &mut Vec<syn::Attribute>) -> Result<BindgenAttrs, Diagnostic> {
        let mut ret = BindgenAttrs::default();
        loop {
            let pos = attrs
                .iter()
                .enumerate()
                .find(|&(_, m)| m.path().segments[0].ident == "wasm_bindgen")
                .map(|a| a.0);
            let pos = match pos {
                Some(i) => i,
                None => return Ok(ret),
            };
            let attr = attrs.remove(pos);
            let tokens = match attr.meta {
                syn::Meta::Path(_) => continue,
                syn::Meta::List(syn::MetaList {
                    delimiter: MacroDelimiter::Paren(_),
                    tokens,
                    ..
                }) => tokens,
                syn::Meta::List(_) | syn::Meta::NameValue(_) => {
                    bail_span!(attr, "malformed #[wasm_bindgen] attribute")
                }
            };
            let mut attrs: BindgenAttrs = syn::parse2(tokens)?;
            ret.attrs.append(&mut attrs.attrs);
            attrs.check_used();
        }
    }

    fn get_thread_local(&self) -> Result<Option<ThreadLocal>, Diagnostic> {
        let mut thread_local = self.thread_local_v2().map(|_| ThreadLocal::V2);

        if let Some(span) = self.thread_local() {
            if thread_local.is_some() {
                return Err(Diagnostic::span_error(
                    *span,
                    "`thread_local` can't be used with `thread_local_v2`",
                ));
            } else {
                thread_local = Some(ThreadLocal::V1)
            }
        }

        Ok(thread_local)
    }

    attrgen!(methods);
}

impl Default for BindgenAttrs {
    fn default() -> BindgenAttrs {
        // Add 1 to the list of parsed attribute sets. We'll use this counter to
        // sanity check that we call `check_used` an appropriate number of
        // times.
        ATTRS.with(|state| state.parsed.set(state.parsed.get() + 1));
        BindgenAttrs { attrs: Vec::new() }
    }
}

impl Parse for BindgenAttrs {
    fn parse(input: ParseStream) -> SynResult<Self> {
        let mut attrs = BindgenAttrs::default();
        if input.is_empty() {
            return Ok(attrs);
        }

        let opts = syn::punctuated::Punctuated::<_, syn::token::Comma>::parse_terminated(input)?;
        attrs.attrs = opts.into_iter().map(|c| (Cell::new(false), c)).collect();
        Ok(attrs)
    }
}

macro_rules! gen_bindgen_attr {
    ($(($method:ident, $_:literal, $($variants:tt)*),)*) => {
        /// The possible attributes in the `#[wasm_bindgen]`.
        #[cfg_attr(feature = "extra-traits", derive(Debug))]
        pub enum BindgenAttr {
            $($($variants)*,)*
        }
    }
}
attrgen!(gen_bindgen_attr);

impl Parse for BindgenAttr {
    fn parse(input: ParseStream) -> SynResult<Self> {
        let original = input.fork();
        let attr: AnyIdent = input.parse()?;
        let attr = attr.0;
        let attr_span = attr.span();
        let attr_string = attr.to_string();
        let raw_attr_string = format!("r#{attr_string}");

        macro_rules! parsers {
            ($(($name:ident, $_:literal, $($contents:tt)*),)*) => {
                $(
                    if attr_string == stringify!($name) || raw_attr_string == stringify!($name) {
                        parsers!(
                            @parser
                            $($contents)*
                        );
                    }
                )*
            };

            (@parser $variant:ident(Span)) => ({
                return Ok(BindgenAttr::$variant(attr_span));
            });

            (@parser $variant:ident(Span, Ident)) => ({
                input.parse::<Token![=]>()?;
                let ident = input.parse::<AnyIdent>()?.0;
                return Ok(BindgenAttr::$variant(attr_span, ident))
            });

            (@parser $variant:ident(Span, Option<String>)) => ({
                if input.parse::<Token![=]>().is_ok() {
                    if input.peek(syn::LitStr) {
                        let litstr = input.parse::<syn::LitStr>()?;
                        return Ok(BindgenAttr::$variant(attr_span, Some(litstr.value())))
                    }

                    let ident = input.parse::<AnyIdent>()?.0;
                    return Ok(BindgenAttr::$variant(attr_span, Some(ident.to_string())))
                } else {
                    return Ok(BindgenAttr::$variant(attr_span, None));
                }
            });

            (@parser $variant:ident(Span, syn::Path)) => ({
                input.parse::<Token![=]>()?;
                return Ok(BindgenAttr::$variant(attr_span, input.parse()?));
            });

            (@parser $variant:ident(Span, syn::Expr)) => ({
                input.parse::<Token![=]>()?;
                return Ok(BindgenAttr::$variant(attr_span, input.parse()?));
            });

            (@parser $variant:ident(Span, String, Span)) => ({
                input.parse::<Token![=]>()?;
                let (val, span) = match input.parse::<syn::LitStr>() {
                    Ok(str) => (str.value(), str.span()),
                    Err(_) => {
                        let ident = input.parse::<AnyIdent>()?.0;
                        (ident.to_string(), ident.span())
                    }
                };
                return Ok(BindgenAttr::$variant(attr_span, val, span))
            });

            (@parser $variant:ident(Span, JsNamespace, Vec<Span>)) => ({
                input.parse::<Token![=]>()?;
                let (vals, spans) = match input.parse::<syn::ExprArray>() {
                    Ok(exprs) => {
                        let mut vals = vec![];
                        let mut spans = vec![];

                        for expr in exprs.elems.iter() {
                            if let syn::Expr::Lit(syn::ExprLit {
                                lit: syn::Lit::Str(ref str),
                                ..
                            }) = expr {
                                vals.push(str.value());
                                spans.push(str.span());
                            } else {
                                return Err(syn::Error::new(expr.span(), "expected string literals"));
                            }
                        }

                        if vals.is_empty() {
                            return Err(syn::Error::new(exprs.span(), "Empty namespace lists are not allowed."));
                        }

                        (vals, spans)
                    },
                    // Try parsing as a string literal, then fall back to identifier
                    Err(_) => match input.parse::<syn::LitStr>() {
                        Ok(str) => (vec![str.value()], vec![str.span()]),
                        Err(_) => {
                            let ident = input.parse::<AnyIdent>()?.0;
                            (vec![ident.to_string()], vec![ident.span()])
                        }
                    }
                };

                let first = &vals[0];
                if is_non_value_js_keyword(first) && first != "default" {
                    let msg = format!("Namespace cannot start with the JS keyword `{}`", first);
                    return Err(syn::Error::new(spans[0], msg));
                }

                return Ok(BindgenAttr::$variant(attr_span, JsNamespace(vals), spans))
            });
        }

        attrgen!(parsers);

        Err(original.error(if attr_string.starts_with('_') {
            "unknown attribute: it's safe to remove unused attributes entirely."
        } else {
            "unknown attribute"
        }))
    }
}

struct AnyIdent(Ident);

impl Parse for AnyIdent {
    fn parse(input: ParseStream) -> SynResult<Self> {
        input.step(|cursor| match cursor.ident() {
            Some((ident, remaining)) => Ok((AnyIdent(ident), remaining)),
            None => Err(cursor.error("expected an identifier")),
        })
    }
}

/// Conversion trait with context.
///
/// Used to convert syn tokens into an AST, that we can then use to generate glue code. The context
/// (`Ctx`) is used to pass in the attributes from the `#[wasm_bindgen]`, if needed.
pub(crate) trait ConvertToAst<Ctx> {
    /// What we are converting to.
    type Target;
    /// Convert into our target.
    ///
    /// Since this is used in a procedural macro, use panic to fail.
    fn convert(self, context: Ctx) -> Result<Self::Target, Diagnostic>;
}

impl ConvertToAst<&ast::Program> for &mut syn::ItemStruct {
    type Target = ast::Struct;

    fn convert(self, program: &ast::Program) -> Result<Self::Target, Diagnostic> {
        if !self.generics.params.is_empty() {
            bail_span!(
                self.generics,
                "structs with #[wasm_bindgen] cannot have lifetime or \
                 type parameters currently"
            );
        }
        let attrs = BindgenAttrs::find(&mut self.attrs)?;

        // the `wasm_bindgen` option has been used before
        let _ = attrs.wasm_bindgen();

        let mut fields = Vec::new();
        let js_name = attrs
            .js_name()
            .map(|s| s.0.to_string())
            .unwrap_or(self.ident.unraw().to_string());
        if is_js_keyword(&js_name) && js_name != "default" {
            bail_span!(
                self.ident,
                "struct cannot use the JS keyword `{}` as its name",
                js_name
            );
        }

        let is_inspectable = attrs.inspectable().is_some();
        let getter_with_clone = attrs.getter_with_clone();
        let js_namespace = attrs.js_namespace().map(|(ns, _)| ns.0);
        let qualified_name = wasm_bindgen_shared::qualified_name(js_namespace.as_deref(), &js_name);
        for (i, field) in self.fields.iter_mut().enumerate() {
            match field.vis {
                syn::Visibility::Public(..) => {}
                _ => continue,
            }
            let (js_field_name, member) = match &field.ident {
                Some(ident) => (ident.unraw().to_string(), syn::Member::Named(ident.clone())),
                None => (i.to_string(), syn::Member::Unnamed(i.into())),
            };

            let attrs = BindgenAttrs::find(&mut field.attrs)?;
            if attrs.skip().is_some() {
                attrs.check_used();
                continue;
            }

            let js_field_name = match attrs.js_name() {
                Some((name, _)) => name.to_string(),
                None => js_field_name,
            };

            let comments = extract_doc_comments(&field.attrs);
            let getter = wasm_bindgen_shared::struct_field_get(&qualified_name, &js_field_name);
            let setter = wasm_bindgen_shared::struct_field_set(&qualified_name, &js_field_name);

            fields.push(ast::StructField {
                rust_name: member,
                js_name: js_field_name,
                struct_name: self.ident.clone(),
                readonly: attrs.readonly().is_some(),
                ty: field.ty.clone(),
                getter: Ident::new(&getter, Span::call_site()),
                setter: Ident::new(&setter, Span::call_site()),
                comments,
                generate_typescript: attrs.skip_typescript().is_none(),
                generate_jsdoc: attrs.skip_jsdoc().is_none(),
                getter_with_clone: attrs.getter_with_clone().or(getter_with_clone).copied(),
                wasm_bindgen: program.wasm_bindgen.clone(),
            });
            attrs.check_used();
        }
        let generate_typescript = attrs.skip_typescript().is_none();
        let private = attrs.private().is_some();
        let comments: Vec<String> = extract_doc_comments(&self.attrs);
        attrs.check_used();
        Ok(ast::Struct {
            rust_name: self.ident.clone(),
            js_name,
            qualified_name,
            fields,
            comments,
            is_inspectable,
            generate_typescript,
            private,
            js_namespace,
            wasm_bindgen: program.wasm_bindgen.clone(),
        })
    }
}

fn get_ty(mut ty: &syn::Type) -> &syn::Type {
    while let syn::Type::Group(g) = ty {
        ty = &g.elem;
    }

    ty
}

fn get_expr(mut expr: &syn::Expr) -> &syn::Expr {
    while let syn::Expr::Group(g) = expr {
        expr = &g.expr;
    }

    expr
}

impl<'a> ConvertToAst<(&ast::Program, BindgenAttrs, &'a Option<ast::ImportModule>)>
    for syn::ForeignItemFn
{
    type Target = ast::ImportKind;

    fn convert(
        self,
        (program, opts, module): (&ast::Program, BindgenAttrs, &'a Option<ast::ImportModule>),
    ) -> Result<Self::Target, Diagnostic> {
        let (mut wasm, _) = function_from_decl(
            &self.sig.ident,
            &opts,
            self.sig.clone(),
            self.attrs.clone(),
            self.vis.clone(),
            FunctionPosition::Extern,
            None,
        )?;
        let catch = opts.catch().is_some();
        let variadic = opts.variadic().is_some();
        let js_ret = if catch {
            // TODO: this assumes a whole bunch:
            //
            // * The outer type is actually a `Result`
            // * The error type is a `JsValue`
            // * The actual type is the first type parameter
            //
            // should probably fix this one day...
            extract_first_ty_param(wasm.ret.as_ref().map(|ret| &ret.r#type))?
        } else {
            wasm.ret.as_ref().map(|ret| ret.r#type.clone())
        };

        let operation_kind = operation_kind(&opts);

        let kind = if opts.method().is_some() {
            let class = wasm.arguments.first().ok_or_else(|| {
                err_span!(self, "imported methods must have at least one argument")
            })?;
            let class = match get_ty(&class.pat_type.ty) {
                syn::Type::Reference(syn::TypeReference {
                    mutability: None,
                    elem,
                    ..
                }) => &**elem,
                _ => bail_span!(
                    class.pat_type.ty,
                    "first argument of method must be a shared reference"
                ),
            };
            let class_ty = get_ty(class);
            let js_class = opts.js_class().map(|p| p.0.to_string());
            let kind = ast::MethodKind::Operation(ast::Operation {
                is_static: false,
                kind: operation_kind,
            });

            let class_name = match class_ty {
                syn::Type::Path(syn::TypePath {
                    qself: None,
                    ref path,
                }) => path,
                _ => bail_span!(class_ty, "first argument of method must be a path"),
            };

            let class_name_str = js_class
                .map(Ok)
                .unwrap_or_else(|| extract_path_ident(class_name, true).map(|i| i.to_string()))?;

            ast::ImportFunctionKind::Method {
                class: class_name_str,
                ty: class_ty.clone(),
                kind,
            }
        } else if let Some(cls) = opts.static_method_of() {
            let class = opts
                .js_class()
                .map(|p| p.0.into())
                .unwrap_or_else(|| cls.to_string());

            let ty = syn::Type::Path(syn::TypePath {
                qself: None,
                path: syn::Path {
                    leading_colon: None,
                    segments: std::iter::once(syn::PathSegment {
                        ident: cls.clone(),
                        arguments: syn::PathArguments::None,
                    })
                    .collect(),
                },
            });

            let kind = ast::MethodKind::Operation(ast::Operation {
                is_static: true,
                kind: operation_kind,
            });

            ast::ImportFunctionKind::Method { class, ty, kind }
        } else if opts.constructor().is_some() {
            let class = match js_ret {
                Some(ref ty) => ty,
                _ => bail_span!(self, "constructor returns must be bare types"),
            };
            let class_name = match get_ty(class) {
                syn::Type::Path(syn::TypePath {
                    qself: None,
                    ref path,
                }) => path,
                _ => bail_span!(self, "return value of constructor must be a bare path"),
            };
            let class_name = extract_path_ident(class_name, true)?;
            let class_name = opts
                .js_class()
                .map(|p| p.0.into())
                .unwrap_or_else(|| class_name.to_string());

            ast::ImportFunctionKind::Method {
                class: class_name,
                ty: class.clone(),
                kind: ast::MethodKind::Constructor,
            }
        } else {
            ast::ImportFunctionKind::Normal
        };

        // Validate that reexport is not used on methods/constructors/static methods
        if opts.reexport().is_some() && matches!(kind, ast::ImportFunctionKind::Method { .. }) {
            return Err(Diagnostic::span_error(
                self.sig.ident.span(),
                "`reexport` cannot be used on methods, constructors, or static methods. \
                Use `reexport` on the type import instead.",
            ));
        }

        let shim = {
            let ns = match kind {
                ast::ImportFunctionKind::Normal => (0, "n"),
                ast::ImportFunctionKind::Method { ref class, .. } => (1, &class[..]),
            };
            // Include cfg attributes in the hash so that functions with different
            // cfg gates get different shim names, even if their signatures are identical.
            let cfg_attrs: String = self
                .attrs
                .iter()
                .filter(|attr| attr.path().is_ident("cfg"))
                .map(|attr| attr.to_token_stream().to_string())
                .collect();
            let data = (
                ns,
                self.sig.to_token_stream().to_string(),
                module,
                cfg_attrs,
            );
            format!(
                "__wbg_{}_{}",
                wasm.name
                    .chars()
                    .filter(|&c| c.is_ascii_alphanumeric() || c == '_')
                    .collect::<String>(),
                ShortHash(data)
            )
        };
        if let Some(span) = opts.r#final() {
            if opts.structural().is_some() {
                let msg = "cannot specify both `structural` and `final`";
                return Err(Diagnostic::span_error(*span, msg));
            }
        }
        let assert_no_shim = opts.assert_no_shim().is_some();

        let mut doc_comment = String::new();
        // Extract the doc comments from our list of attributes.
        wasm.rust_attrs.retain(|attr| {
            /// Returns the contents of the passed `#[doc = "..."]` attribute,
            /// or `None` if it isn't one.
            fn get_docs(attr: &syn::Attribute) -> Option<String> {
                if attr.path().is_ident("doc") {
                    if let syn::Meta::NameValue(syn::MetaNameValue {
                        value:
                            syn::Expr::Lit(syn::ExprLit {
                                lit: Lit::Str(str), ..
                            }),
                        ..
                    }) = &attr.meta
                    {
                        Some(str.value())
                    } else {
                        None
                    }
                } else {
                    None
                }
            }

            if let Some(docs) = get_docs(attr) {
                if !doc_comment.is_empty() {
                    // Add newlines between the doc comments
                    doc_comment.push('\n');
                }
                // Add this doc comment to the complete docs
                doc_comment.push_str(&docs);

                // Remove it from the list of regular attributes
                false
            } else {
                true
            }
        });

        validate_generics(&self.sig.generics)?;

        let ret = ast::ImportKind::Function(ast::ImportFunction {
            function: wasm,
            assert_no_shim,
            kind,
            js_ret,
            catch,
            variadic,
            structural: opts.structural().is_some() || opts.r#final().is_none(),
            rust_name: self.sig.ident,
            shim: Ident::new(&shim, Span::call_site()),
            doc_comment,
            wasm_bindgen: program.wasm_bindgen.clone(),
            wasm_bindgen_futures: program.wasm_bindgen_futures.clone(),
            generics: self.sig.generics,
        });
        opts.check_used();

        Ok(ret)
    }
}

impl ConvertToAst<(&ast::Program, BindgenAttrs)> for syn::ForeignItemType {
    type Target = ast::ImportKind;

    fn convert(
        self,
        (program, attrs): (&ast::Program, BindgenAttrs),
    ) -> Result<Self::Target, Diagnostic> {
        let js_name = attrs
            .js_name()
            .map(|s| s.0)
            .map_or_else(|| self.ident.to_string(), |s| s.to_string());
        let typescript_type = attrs.typescript_type().map(|s| s.0.to_string());
        let is_type_of = attrs.is_type_of().cloned();
        let shim = format!(
            "__wbg_instanceof_{}_{}",
            self.ident,
            ShortHash((attrs.js_namespace().map(|(ns, _)| ns.0), &self.ident))
        );
        let mut extends = Vec::new();
        let mut vendor_prefixes = Vec::new();
        let no_deref = attrs.no_deref().is_some();
        let no_upcast = attrs.no_upcast().is_some();
        let no_promising = attrs.no_promising().is_some();
        for (used, attr) in attrs.attrs.iter() {
            match attr {
                BindgenAttr::Extends(_, e) => {
                    extends.push(e.clone());
                    used.set(true);
                }
                BindgenAttr::VendorPrefix(_, e) => {
                    vendor_prefixes.push(e.clone());
                    used.set(true);
                }
                _ => {}
            }
        }

        attrs.check_used();
        validate_generics(&self.generics)?;

        // Ensure defaults are set for all generic type params on imported class definitions.
        // JsValue as the default default.
        let mut generics = None;
        for (n, param) in self.generics.type_params().enumerate() {
            if param.default.is_none() {
                let generics = generics.get_or_insert_with(|| self.generics.clone());
                let type_param_mut = generics.type_params_mut().nth(n).unwrap();
                type_param_mut.default = Some(syn::parse_quote! { JsValue });
            }
        }

        Ok(ast::ImportKind::Type(ast::ImportType {
            vis: self.vis,
            attrs: self.attrs,
            doc_comment: None,
            instanceof_shim: shim,
            is_type_of,
            rust_name: self.ident,
            typescript_type,
            js_name,
            extends,
            vendor_prefixes,
            no_deref,
            no_upcast,
            no_promising,
            wasm_bindgen: program.wasm_bindgen.clone(),
            generics: generics.unwrap_or(self.generics),
        }))
    }
}

impl<'a> ConvertToAst<(&ast::Program, BindgenAttrs, &'a Option<ast::ImportModule>)>
    for syn::ForeignItemStatic
{
    type Target = ast::ImportKind;

    fn convert(
        self,
        (program, opts, module): (&ast::Program, BindgenAttrs, &'a Option<ast::ImportModule>),
    ) -> Result<Self::Target, Diagnostic> {
        if let syn::StaticMutability::Mut(_) = self.mutability {
            bail_span!(self.mutability, "cannot import mutable globals yet")
        }

        if let Some(span) = opts.static_string() {
            return Err(Diagnostic::span_error(
                *span,
                "static strings require a string literal",
            ));
        }

        let default_name = self.ident.to_string();
        let js_name = opts
            .js_name()
            .map(|p| p.0)
            .unwrap_or(&default_name)
            .to_string();
        let shim = format!(
            "__wbg_static_accessor_{}_{}",
            self.ident,
            ShortHash((&js_name, module, &self.ident)),
        );
        let thread_local = opts.get_thread_local()?;

        opts.check_used();
        Ok(ast::ImportKind::Static(ast::ImportStatic {
            ty: *self.ty,
            vis: self.vis,
            rust_name: self.ident.clone(),
            js_name,
            shim: Ident::new(&shim, Span::call_site()),
            wasm_bindgen: program.wasm_bindgen.clone(),
            thread_local,
        }))
    }
}

impl<'a> ConvertToAst<(&ast::Program, BindgenAttrs, &'a Option<ast::ImportModule>)>
    for syn::ItemStatic
{
    type Target = ast::ImportKind;

    fn convert(
        self,
        (program, opts, module): (&ast::Program, BindgenAttrs, &'a Option<ast::ImportModule>),
    ) -> Result<Self::Target, Diagnostic> {
        if let syn::StaticMutability::Mut(_) = self.mutability {
            bail_span!(self.mutability, "cannot import mutable globals yet")
        }

        let string = if let syn::Expr::Lit(syn::ExprLit {
            lit: syn::Lit::Str(string),
            ..
        }) = *self.expr.clone()
        {
            string.value()
        } else {
            bail_span!(
                self.expr,
                "statics with a value can only be string literals"
            )
        };

        if opts.static_string().is_none() {
            bail_span!(
                self,
                "static strings require `#[wasm_bindgen(static_string)]`"
            )
        }

        let thread_local = if let Some(thread_local) = opts.get_thread_local()? {
            thread_local
        } else {
            bail_span!(
                self,
                "static strings require `#[wasm_bindgen(thread_local_v2)]`"
            )
        };

        let shim = format!(
            "__wbg_string_{}_{}",
            self.ident,
            ShortHash((&module, &self.ident)),
        );
        opts.check_used();
        Ok(ast::ImportKind::String(ast::ImportString {
            ty: *self.ty,
            vis: self.vis,
            rust_name: self.ident.clone(),
            shim: Ident::new(&shim, Span::call_site()),
            wasm_bindgen: program.wasm_bindgen.clone(),
            js_sys: program.js_sys.clone(),
            string,
            thread_local,
        }))
    }
}

impl ConvertToAst<(BindgenAttrs, Vec<FnArgAttrs>)> for syn::ItemFn {
    type Target = ast::Function;

    fn convert(
        self,
        (attrs, args_attrs): (BindgenAttrs, Vec<FnArgAttrs>),
    ) -> Result<Self::Target, Diagnostic> {
        match self.vis {
            syn::Visibility::Public(_) => {}
            _ if attrs.start().is_some() => {}
            _ => bail_span!(self, "can only #[wasm_bindgen] public functions"),
        }
        if self.sig.constness.is_some() {
            bail_span!(
                self.sig.constness,
                "can only #[wasm_bindgen] non-const functions"
            );
        }

        let (mut ret, _) = function_from_decl(
            &self.sig.ident,
            &attrs,
            self.sig.clone(),
            self.attrs,
            self.vis,
            FunctionPosition::Free,
            Some(args_attrs),
        )?;
        attrs.check_used();

        // TODO: Deprecate this for next major
        // Due to legacy behavior, we need to escape all keyword identifiers as
        // `_keyword`, except `default`
        if is_js_keyword(&ret.name) && ret.name != "default" {
            ret.name = format!("_{}", ret.name);
        }

        Ok(ret)
    }
}

/// Returns whether `self` is passed by reference or by value.
fn get_self_method(r: syn::Receiver) -> ast::MethodSelf {
    // The tricky part here is that `r` can have many forms. E.g. `self`,
    // `&self`, `&mut self`, `self: Self`, `self: &Self`, `self: &mut Self`,
    // `self: Box<Self>`, `self: Rc<Self>`, etc.
    // Luckily, syn always populates the `ty` field with the type of `self`, so
    // e.g. `&self` gets the type `&Self`. So we only have check whether the
    // type is a reference or not.
    match &*r.ty {
        syn::Type::Reference(ty) => {
            if ty.mutability.is_some() {
                ast::MethodSelf::RefMutable
            } else {
                ast::MethodSelf::RefShared
            }
        }
        _ => ast::MethodSelf::ByValue,
    }
}

enum FunctionPosition<'a> {
    Extern,
    Free,
    Impl { self_ty: &'a Ident },
}

/// Construct a function (and gets the self type if appropriate) for our AST from a syn function.
#[allow(clippy::too_many_arguments)]
fn function_from_decl(
    decl_name: &syn::Ident,
    opts: &BindgenAttrs,
    sig: syn::Signature,
    attrs: Vec<syn::Attribute>,
    vis: syn::Visibility,
    position: FunctionPosition,
    args_attrs: Option<Vec<FnArgAttrs>>,
) -> Result<(ast::Function, Option<ast::MethodSelf>), Diagnostic> {
    if sig.variadic.is_some() {
        bail_span!(sig.variadic, "can't #[wasm_bindgen] variadic functions");
    }

    // For imported functions (Extern position), generics are supported and validated.
    if !matches!(position, FunctionPosition::Extern) && !sig.generics.params.is_empty() {
        bail_span!(
            sig.generics,
            "can't #[wasm_bindgen] functions with lifetime or type parameters"
        );
    }

    let syn::Signature { inputs, output, .. } = sig;

    // A helper function to replace `Self` in the function signature of methods.
    // E.g. `fn get(&self) -> Option<Self>` to `fn get(&self) -> Option<MyType>`
    // The following comment explains why this is necessary:
    // https://github.com/wasm-bindgen/wasm-bindgen/issues/3105#issuecomment-1275160744
    let replace_self = |mut t: syn::Type| {
        if let FunctionPosition::Impl { self_ty } = position {
            // This uses a visitor to replace all occurrences of `Self` with
            // the actual type identifier. The visitor guarantees that we find
            // all occurrences of `Self`, even if deeply nested and even if
            // future Rust versions add more places where `Self` can appear.
            struct SelfReplace(Ident);
            impl VisitMut for SelfReplace {
                fn visit_ident_mut(&mut self, i: &mut proc_macro2::Ident) {
                    if i == "Self" {
                        *i = self.0.clone();
                    }
                }
            }

            let mut replace = SelfReplace(self_ty.clone());
            replace.visit_type_mut(&mut t);
        }
        t
    };

    // A helper function to replace argument names that are JS keywords.
    // E.g. this will replace `fn foo(class: u32)` to `fn foo(_class: u32)`
    let replace_colliding_arg = |i: &mut syn::PatType| {
        if let syn::Pat::Ident(ref mut i) = *i.pat {
            let ident = i.ident.unraw().to_string();
            // JS keywords are NEVER allowed as argument names. Since argument
            // names are considered an implementation detail in JS, we can
            // safely rename them to avoid collisions.
            if is_js_keyword(&ident) {
                i.ident = Ident::new(format!("_{ident}").as_str(), i.ident.span());
            }
        }
    };

    let mut method_self = None;
    let mut arguments = Vec::new();
    for arg in inputs.into_iter() {
        match arg {
            syn::FnArg::Typed(mut c) => {
                // typical arguments like foo: u32
                replace_colliding_arg(&mut c);
                *c.ty = replace_self(*c.ty);
                arguments.push(c);
            }
            syn::FnArg::Receiver(r) => {
                // the self argument, so self, &self, &mut self, self: Box<Self>, etc.

                // `self` is only allowed for `fn`s inside an `impl` block.
                match position {
                    FunctionPosition::Free => {
                        bail_span!(
                            r.self_token,
                            "the `self` argument is only allowed for functions in `impl` blocks.\n\n\
                            If the function is already in an `impl` block, did you perhaps forget to add `#[wasm_bindgen]` to the `impl` block?"
                        );
                    }
                    FunctionPosition::Extern => {
                        bail_span!(
                            r.self_token,
                            "the `self` argument is not allowed for `extern` functions.\n\n\
                            Did you perhaps mean `this`? For more information on importing JavaScript functions, see:\n\
                            https://wasm-bindgen.github.io/wasm-bindgen/examples/import-js.html"
                        );
                    }
                    FunctionPosition::Impl { .. } => {}
                }

                // We need to know *how* `self` is passed to the method (by
                // value or by reference) to generate the correct JS shim.
                assert!(method_self.is_none());
                method_self = Some(get_self_method(r));
            }
        }
    }

    // process function return data
    let ret_ty_override = opts.unchecked_return_type();
    let ret_desc = opts.return_description();
    let ret = match output {
        syn::ReturnType::Default => None,
        syn::ReturnType::Type(_, ty) => Some(ast::FunctionReturnData {
            r#type: replace_self(*ty),
            js_type: ret_ty_override
                .as_ref()
                .map_or::<Result<_, Diagnostic>, _>(Ok(None), |(ty, span)| {
                    check_invalid_type(ty, *span)?;
                    Ok(Some(ty.to_string()))
                })?,
            desc: ret_desc.as_ref().map_or::<Result<_, Diagnostic>, _>(
                Ok(None),
                |(desc, span)| {
                    check_js_comment_close(desc, *span)?;
                    Ok(Some(desc.to_string()))
                },
            )?,
        }),
    };
    // error if there were description or type override specified for
    // function return while it doesn't actually return anything
    if ret.is_none() && (ret_ty_override.is_some() || ret_desc.is_some()) {
        if let Some((_, span)) = ret_ty_override {
            return Err(Diagnostic::span_error(
                span,
                "cannot specify return type for a function that doesn't return",
            ));
        }
        if let Some((_, span)) = ret_desc {
            return Err(Diagnostic::span_error(
                span,
                "cannot specify return description for a function that doesn't return",
            ));
        }
    }

    let (name, name_span) = if let Some((js_name, js_name_span)) = opts.js_name() {
        let kind = operation_kind(opts);
        let prefix = match kind {
            OperationKind::Setter(_) => "set_",
            _ => "",
        };
        (format!("{prefix}{js_name}"), js_name_span)
    } else {
        (decl_name.unraw().to_string(), decl_name.span())
    };

    Ok((
        ast::Function {
            name_span,
            name,
            rust_attrs: attrs,
            rust_vis: vis,
            r#unsafe: sig.unsafety.is_some(),
            r#async: sig.asyncness.is_some(),
            generate_typescript: opts.skip_typescript().is_none(),
            generate_jsdoc: opts.skip_jsdoc().is_none(),
            variadic: opts.variadic().is_some(),
            ret,
            arguments: arguments
                .into_iter()
                .zip(
                    args_attrs
                        .into_iter()
                        .flatten()
                        .chain(iter::repeat(FnArgAttrs::default())),
                )
                .map(|(pat_type, attrs)| ast::FunctionArgumentData {
                    pat_type,
                    js_name: attrs.js_name,
                    js_type: attrs.js_type,
                    optional: attrs.optional,
                    desc: attrs.desc,
                })
                .collect(),
        },
        method_self,
    ))
}

/// Helper struct to store extracted function argument attrs
#[derive(Default, Clone)]
struct FnArgAttrs {
    js_name: Option<String>,
    js_type: Option<String>,
    optional: bool,
    desc: Option<String>,
}

/// Extracts function arguments attributes
fn extract_args_attrs(sig: &mut syn::Signature) -> Result<Vec<FnArgAttrs>, Diagnostic> {
    let mut args_attrs = vec![];
    let mut seen_optional: Option<Span> = None;
    for input in sig.inputs.iter_mut() {
        if let syn::FnArg::Typed(pat_type) = input {
            let attrs = BindgenAttrs::find(&mut pat_type.attrs)?;

            // Check for mutually exclusive param type attributes
            let param_type = attrs.unchecked_param_type();
            let optional_param_type = attrs.unchecked_optional_param_type();

            if param_type.is_some() && optional_param_type.is_some() {
                // Find the positions and spans of both attributes in the attrs list
                let mut param_pos_and_span: Option<(usize, Span)> = None;
                let mut optional_pos_and_span: Option<(usize, Span)> = None;
                for (pos, (_, attr)) in attrs.attrs.iter().enumerate() {
                    match attr {
                        BindgenAttr::ParamType(span, _, _) => {
                            param_pos_and_span = Some((pos, *span));
                        }
                        BindgenAttr::OptionalParamType(span, _, _) => {
                            optional_pos_and_span = Some((pos, *span));
                        }
                        _ => {}
                    }
                }
                // Report error at the position of the attribute that appears later
                let error_span = match (param_pos_and_span, optional_pos_and_span) {
                    (Some((p_pos, p_span)), Some((o_pos, o_span))) => {
                        if p_pos > o_pos {
                            p_span
                        } else {
                            o_span
                        }
                    }
                    (Some((_, p_span)), None) => p_span,
                    (None, Some((_, o_span))) => o_span,
                    (None, None) => unreachable!(
                        "both param_type and optional_param_type are Some, but attrs not found"
                    ),
                };
                return Err(Diagnostic::span_error(
                    error_span,
                    "cannot use both `unchecked_param_type` and `unchecked_optional_param_type` on the same parameter",
                ));
            }

            // Determine the type and whether it's optional
            let js_type = param_type
                .or(optional_param_type)
                .map_or::<Result<_, Diagnostic>, _>(Ok(None), |(ty, span)| {
                    check_invalid_type(ty, span)?;
                    Ok(Some(ty.to_string()))
                })?;

            let is_optional = optional_param_type.is_some();

            // Check that a non-optional param doesn't follow an optional one
            if let Some(optional_span) = seen_optional {
                if !is_optional {
                    return Err(Diagnostic::span_error(
                        optional_span,
                        "a required parameter cannot follow an optional parameter",
                    ));
                }
            }
            if is_optional {
                if let Some((_, span)) = optional_param_type {
                    seen_optional = Some(span);
                }
            }

            let arg_attrs = FnArgAttrs {
                js_name: attrs
                    .js_name()
                    .map_or(Ok(None), |(js_name_override, span)| {
                        if is_js_keyword(js_name_override) || !is_valid_ident(js_name_override) {
                            return Err(Diagnostic::span_error(span, "invalid JS identifier"));
                        }
                        Ok(Some(js_name_override.to_string()))
                    })?,
                js_type,
                optional: is_optional,
                desc: attrs
                    .param_description()
                    .map_or::<Result<_, Diagnostic>, _>(Ok(None), |(description, span)| {
                        check_js_comment_close(description, span)?;
                        Ok(Some(description.to_string()))
                    })?,
            };
            // throw error for any unused attrs
            attrs.enforce_used()?;
            args_attrs.push(arg_attrs);
        }
    }
    Ok(args_attrs)
}

pub(crate) trait MacroParse<Ctx> {
    /// Parse the contents of an object into our AST, with a context if necessary.
    ///
    /// The context is used to have access to the attributes on `#[wasm_bindgen]`, and to allow
    /// writing to the output `TokenStream`.
    fn macro_parse(self, program: &mut ast::Program, context: Ctx) -> Result<(), Diagnostic>;
}

impl<'a> MacroParse<(Option<BindgenAttrs>, &'a mut TokenStream)> for syn::Item {
    fn macro_parse(
        self,
        program: &mut ast::Program,
        (opts, tokens): (Option<BindgenAttrs>, &'a mut TokenStream),
    ) -> Result<(), Diagnostic> {
        match self {
            syn::Item::Fn(mut f) => {
                let opts = opts.unwrap_or_default();
                if let Some(path) = opts.wasm_bindgen() {
                    program.wasm_bindgen = path.clone();
                }
                if let Some(path) = opts.js_sys() {
                    program.js_sys = path.clone();
                }
                if let Some(path) = opts.wasm_bindgen_futures() {
                    program.wasm_bindgen_futures = path.clone();
                }

                if opts.main().is_some() {
                    opts.check_used();
                    return main(program, f, tokens);
                }

                let no_mangle = f
                    .attrs
                    .iter()
                    .enumerate()
                    .find(|(_, m)| m.path().is_ident("no_mangle"));
                if let Some((i, _)) = no_mangle {
                    f.attrs.remove(i);
                }
                // extract fn args attributes before parsing to tokens stream
                let args_attrs = extract_args_attrs(&mut f.sig)?;
                let comments = extract_doc_comments(&f.attrs);
                // If the function isn't used for anything other than being exported to JS,
                // it'll be unused when not building for the Wasm target and produce a
                // `dead_code` warning. So, add `#[allow(dead_code)]` before it to avoid that.
                tokens.extend(quote::quote! { #[allow(dead_code)] });
                f.to_tokens(tokens);
                if opts.start().is_some() {
                    if !f.sig.generics.params.is_empty() {
                        bail_span!(&f.sig.generics, "the start function cannot have generics",);
                    }
                    if !f.sig.inputs.is_empty() {
                        bail_span!(&f.sig.inputs, "the start function cannot have arguments",);
                    }
                }
                let method_kind = ast::MethodKind::Operation(ast::Operation {
                    is_static: true,
                    kind: operation_kind(&opts),
                });
                let rust_name = f.sig.ident.clone();
                let start = opts.start().is_some();

                if opts.this().is_some() && f.sig.inputs.is_empty() {
                    bail_span!(
                        &f.sig.inputs,
                        "functions taking a 'this' argument must have at least one parameter"
                    );
                }

                let js_namespace = opts.js_namespace().map(|(ns, _)| ns.0);
                program.exports.push(ast::Export {
                    comments,
                    function: f.convert((opts, args_attrs))?,
                    js_class: None,
                    js_namespace,
                    method_kind,
                    method_self: None,
                    rust_class: None,
                    rust_name,
                    start,
                    wasm_bindgen: program.wasm_bindgen.clone(),
                    wasm_bindgen_futures: program.wasm_bindgen_futures.clone(),
                });
            }
            syn::Item::Impl(mut i) => {
                let opts = opts.unwrap_or_default();
                (&mut i).macro_parse(program, opts)?;
                i.to_tokens(tokens);
            }
            syn::Item::ForeignMod(mut f) => {
                let opts = match opts {
                    Some(opts) => opts,
                    None => BindgenAttrs::find(&mut f.attrs)?,
                };
                f.macro_parse(program, opts)?;
            }
            syn::Item::Enum(mut e) => {
                let opts = match opts {
                    Some(opts) => opts,
                    None => BindgenAttrs::find(&mut e.attrs)?,
                };
                e.macro_parse(program, (tokens, opts))?;
            }
            syn::Item::Const(mut c) => {
                let opts = match opts {
                    Some(opts) => opts,
                    None => BindgenAttrs::find(&mut c.attrs)?,
                };
                c.macro_parse(program, opts)?;
            }
            _ => {
                bail_span!(
                    self,
                    "#[wasm_bindgen] can only be applied to a function, \
                     struct, enum, impl, or extern block",
                );
            }
        }

        Ok(())
    }
}

impl MacroParse<BindgenAttrs> for &mut syn::ItemImpl {
    fn macro_parse(self, program: &mut ast::Program, opts: BindgenAttrs) -> Result<(), Diagnostic> {
        if self.defaultness.is_some() {
            bail_span!(
                self.defaultness,
                "#[wasm_bindgen] default impls are not supported"
            );
        }
        if self.unsafety.is_some() {
            bail_span!(
                self.unsafety,
                "#[wasm_bindgen] unsafe impls are not supported"
            );
        }
        if let Some((_, path, _)) = &self.trait_ {
            bail_span!(path, "#[wasm_bindgen] trait impls are not supported");
        }
        if !self.generics.params.is_empty() {
            bail_span!(
                self.generics,
                "#[wasm_bindgen] generic impls aren't supported"
            );
        }
        let name = match get_ty(&self.self_ty) {
            syn::Type::Path(syn::TypePath {
                qself: None,
                ref path,
            }) => path,
            _ => bail_span!(
                self.self_ty,
                "unsupported self type in #[wasm_bindgen] impl"
            ),
        };
        let mut errors = Vec::new();
        for item in self.items.iter_mut() {
            if let Err(e) = prepare_for_impl_recursion(item, name, program, &opts) {
                errors.push(e);
            }
        }
        Diagnostic::from_vec(errors)?;
        opts.check_used();
        Ok(())
    }
}

// Prepare for recursion into an `impl` block. Here we want to attach an
// internal attribute, `__wasm_bindgen_class_marker`, with any metadata we need
// to pass from the impl to the impl item. Recursive macro expansion will then
// expand the `__wasm_bindgen_class_marker` attribute.
//
// Note that we currently do this because inner items may have things like cfgs
// on them, so we want to expand the impl first, let the insides get cfg'd, and
// then go for the rest.
fn prepare_for_impl_recursion(
    item: &mut syn::ImplItem,
    class: &syn::Path,
    program: &ast::Program,
    impl_opts: &BindgenAttrs,
) -> Result<(), Diagnostic> {
    let method = match item {
        syn::ImplItem::Fn(m) => m,
        syn::ImplItem::Const(_) => {
            bail_span!(
                &*item,
                "const definitions aren't supported with #[wasm_bindgen]"
            );
        }
        syn::ImplItem::Type(_) => bail_span!(
            &*item,
            "type definitions in impls aren't supported with #[wasm_bindgen]"
        ),
        syn::ImplItem::Macro(_) => {
            // In theory we want to allow this, but we have no way of expanding
            // the macro and then placing our magical attributes on the expanded
            // functions. As a result, just disallow it for now to hopefully
            // ward off buggy results from this macro.
            bail_span!(&*item, "macros in impls aren't supported");
        }
        syn::ImplItem::Verbatim(_) => panic!("unparsed impl item?"),
        other => bail_span!(other, "failed to parse this item as a known item"),
    };

    let ident = extract_path_ident(class, false)?;

    let js_class = impl_opts
        .js_class()
        .map(|s| s.0.to_string())
        .unwrap_or(ident.to_string());

    let wasm_bindgen = &program.wasm_bindgen;
    let wasm_bindgen_futures = &program.wasm_bindgen_futures;
    method.attrs.insert(
        0,
        syn::Attribute {
            pound_token: Default::default(),
            style: syn::AttrStyle::Outer,
            bracket_token: Default::default(),
            meta: syn::parse_quote! { #wasm_bindgen::prelude::__wasm_bindgen_class_marker(#class = #js_class, wasm_bindgen = #wasm_bindgen, wasm_bindgen_futures = #wasm_bindgen_futures) },
        },
    );

    Ok(())
}

impl MacroParse<&ClassMarker> for &mut syn::ImplItemFn {
    fn macro_parse(
        self,
        program: &mut ast::Program,
        ClassMarker {
            class,
            js_class,
            wasm_bindgen,
            wasm_bindgen_futures,
        }: &ClassMarker,
    ) -> Result<(), Diagnostic> {
        program.wasm_bindgen = wasm_bindgen.clone();
        program.wasm_bindgen_futures = wasm_bindgen_futures.clone();

        match self.vis {
            syn::Visibility::Public(_) => {}
            _ => return Ok(()),
        }
        if self.defaultness.is_some() {
            panic!("default methods are not supported");
        }
        if self.sig.constness.is_some() {
            bail_span!(
                self.sig.constness,
                "can only #[wasm_bindgen] non-const functions",
            );
        }

        let opts = BindgenAttrs::find(&mut self.attrs)?;

        if opts.this().is_some() {
            bail_span!(
                &self.sig.ident,
                "#[wasm_bindgen(this)] cannot be used on impl block methods; \
                 it is only valid on free functions"
            );
        }

        let comments = extract_doc_comments(&self.attrs);
        let args_attrs: Vec<FnArgAttrs> = extract_args_attrs(&mut self.sig)?;
        let (function, method_self) = function_from_decl(
            &self.sig.ident,
            &opts,
            self.sig.clone(),
            self.attrs.clone(),
            self.vis.clone(),
            FunctionPosition::Impl { self_ty: class },
            Some(args_attrs),
        )?;
        let method_kind = if opts.constructor().is_some() {
            ast::MethodKind::Constructor
        } else {
            let is_static = method_self.is_none();
            let kind = operation_kind(&opts);
            ast::MethodKind::Operation(ast::Operation { is_static, kind })
        };

        // Validate that js_namespace is not used on methods
        if let Some((_, span)) = opts.js_namespace() {
            return Err(Diagnostic::span_error(
                span[0],
                "`js_namespace` cannot be used on methods, getters, setters, or static methods. \
                Use `js_namespace` on the exported struct definition instead to put the entire class in a namespace.",
            ));
        }

        program.exports.push(ast::Export {
            comments,
            function,
            js_class: Some(js_class.to_string()),
            js_namespace: None,
            method_kind,
            method_self,
            rust_class: Some(class.clone()),
            rust_name: self.sig.ident.clone(),
            start: false,
            wasm_bindgen: program.wasm_bindgen.clone(),
            wasm_bindgen_futures: program.wasm_bindgen_futures.clone(),
        });
        opts.check_used();
        Ok(())
    }
}

fn string_enum(
    enum_: syn::ItemEnum,
    program: &mut ast::Program,
    js_name: String,
    generate_typescript: bool,
    comments: Vec<String>,
    js_namespace: Option<Vec<String>>,
) -> Result<(), Diagnostic> {
    let mut variants = vec![];
    let mut variant_values = vec![];

    for v in enum_.variants.iter() {
        let (_, expr) = match &v.discriminant {
            Some(pair) => pair,
            None => {
                bail_span!(v, "all variants of a string enum must have a string value");
            }
        };
        match get_expr(expr) {
            syn::Expr::Lit(syn::ExprLit {
                attrs: _,
                lit: syn::Lit::Str(str_lit),
            }) => {
                variants.push(v.ident.clone());
                variant_values.push(str_lit.value());
            }
            expr => bail_span!(
                expr,
                "enums with #[wasm_bindgen] cannot mix string and non-string values",
            ),
        }
    }

    program.imports.push(ast::Import {
        module: None,
        js_namespace: None,
        reexport: None,
        kind: ast::ImportKind::Enum(ast::StringEnum {
            vis: enum_.vis,
            name: enum_.ident,
            export_name: js_name,
            variants,
            variant_values,
            comments,
            rust_attrs: enum_.attrs,
            generate_typescript,
            js_namespace,
            wasm_bindgen: program.wasm_bindgen.clone(),
        }),
    });

    Ok(())
}

/// Represents a possibly negative numeric value as base 10 digits.
struct NumericValue<'a> {
    negative: bool,
    base10_digits: &'a str,
}
impl<'a> NumericValue<'a> {
    fn from_expr(expr: &'a syn::Expr) -> Option<Self> {
        match get_expr(expr) {
            syn::Expr::Lit(syn::ExprLit {
                lit: syn::Lit::Int(int_lit),
                ..
            }) => Some(Self {
                negative: false,
                base10_digits: int_lit.base10_digits(),
            }),
            syn::Expr::Unary(syn::ExprUnary {
                op: syn::UnOp::Neg(_),
                expr,
                ..
            }) => Self::from_expr(expr).map(|n| n.neg()),
            _ => None,
        }
    }

    fn parse(&self) -> Option<i64> {
        let mut value = self.base10_digits.parse::<i64>().ok()?;
        if self.negative {
            value = -value;
        }
        Some(value)
    }

    fn neg(self) -> Self {
        Self {
            negative: !self.negative,
            base10_digits: self.base10_digits,
        }
    }
}

impl<'a> MacroParse<(&'a mut TokenStream, BindgenAttrs)> for syn::ItemEnum {
    fn macro_parse(
        self,
        program: &mut ast::Program,
        (tokens, opts): (&'a mut TokenStream, BindgenAttrs),
    ) -> Result<(), Diagnostic> {
        if self.variants.is_empty() {
            bail_span!(self, "cannot export empty enums to JS");
        }
        for variant in self.variants.iter() {
            match variant.fields {
                syn::Fields::Unit => (),
                _ => bail_span!(
                    variant.fields,
                    "enum variants with associated data are not supported with #[wasm_bindgen]"
                ),
            }
        }

        let generate_typescript = opts.skip_typescript().is_none();
        let private = opts.private().is_some();
        let comments = extract_doc_comments(&self.attrs);
        let js_name = opts
            .js_name()
            .map(|s| s.0)
            .map_or_else(|| self.ident.to_string(), |s| s.to_string());
        if is_js_keyword(&js_name) && js_name != "default" {
            bail_span!(
                self.ident,
                "enum cannot use the JS keyword `{}` as its name",
                js_name
            );
        }

        let js_namespace = opts.js_namespace().map(|(ns, _)| ns.0);
        opts.check_used();

        // Check if the enum is a string enum, by checking whether any variant has a string discriminant.
        let is_string_enum = self.variants.iter().any(|v| {
            if let Some((_, expr)) = &v.discriminant {
                if let syn::Expr::Lit(syn::ExprLit {
                    lit: syn::Lit::Str(_),
                    ..
                }) = get_expr(expr)
                {
                    return true;
                }
            }
            false
        });
        if is_string_enum {
            return string_enum(
                self,
                program,
                js_name,
                generate_typescript,
                comments,
                js_namespace,
            );
        }

        match self.vis {
            syn::Visibility::Public(_) => {}
            _ => bail_span!(self, "only public enums are allowed with #[wasm_bindgen]"),
        }

        // Go through all variants once first to determine whether the enum is
        // signed or unsigned. We don't need to actually parse the discriminant
        // values yet, we just need to know their sign. The actual parsing is
        // done in a second pass.
        let signed = self.variants.iter().any(|v| match &v.discriminant {
            Some((_, expr)) => NumericValue::from_expr(expr).is_some_and(|n| n.negative),
            None => false,
        });
        let underlying_min = if signed { i32::MIN as i64 } else { 0 };
        let underlying_max = if signed {
            i32::MAX as i64
        } else {
            u32::MAX as i64
        };

        let mut last_discriminant: Option<i64> = None;
        let mut discriminant_map: HashMap<i64, &syn::Variant> = HashMap::new();

        let variants = self
            .variants
            .iter()
            .map(|v| {
                let value: i64 = match &v.discriminant {
                    Some((_, expr)) => match NumericValue::from_expr(expr).and_then(|n| n.parse()) {
                        Some(value) => value,
                        _ => bail_span!(
                            expr,
                            "C-style enums with #[wasm_bindgen] may only have \
                             numeric literal values that fit in a 32-bit integer as discriminants. \
                             Expressions or variables are not supported.",
                        ),
                    },
                    None => {
                        // Use the same algorithm as rustc to determine the next discriminant
                        // https://doc.rust-lang.org/reference/items/enumerations.html#implicit-discriminants
                        last_discriminant.map_or(0, |last| last + 1)
                    }
                };

                last_discriminant = Some(value);

                // check that the value fits within the underlying type
                let underlying = if signed { "i32" } else { "u32" };
                let numbers = if signed { "signed numbers" } else { "unsigned numbers" };
                if value < underlying_min {
                    bail_span!(
                        v,
                        "C-style enums with #[wasm_bindgen] can only support {0} that can be represented by `{2}`, \
                        but `{1}` is too small for `{2}`",
                        numbers,
                        value,
                        underlying
                    );
                }
                if value > underlying_max {
                    bail_span!(
                        v,
                        "C-style enums with #[wasm_bindgen] can only support {0} that can be represented by `{2}`, \
                        but `{1}` is too large for `{2}`",
                        numbers,
                        value,
                        underlying
                    );
                }

                // detect duplicate discriminants
                if let Some(old) = discriminant_map.insert(value, v) {
                    bail_span!(
                        v,
                        "discriminant value `{}` is already used by {} in this enum",
                        value,
                        old.ident
                    );
                }

                let comments = extract_doc_comments(&v.attrs);
                Ok(ast::Variant {
                    name: v.ident.clone(),
                    // due to the above checks, we know that the value fits
                    // within 32 bits, so this cast doesn't lose any information
                    value: value as u32,
                    comments,
                })
            })
            .collect::<Result<Vec<_>, Diagnostic>>()?;

        // To make all the code handling holes simpler, we only consider
        // non-negative holes. This allows us to use `u32` to represent holes.
        let hole = (0..=underlying_max)
            .find(|v| !discriminant_map.contains_key(v))
            .unwrap() as u32;

        self.to_tokens(tokens);

        program.enums.push(ast::Enum {
            rust_name: self.ident,
            js_name,
            signed,
            variants,
            comments,
            hole,
            generate_typescript,
            private,
            js_namespace,
            wasm_bindgen: program.wasm_bindgen.clone(),
        });
        Ok(())
    }
}

impl MacroParse<BindgenAttrs> for syn::ItemConst {
    fn macro_parse(self, program: &mut ast::Program, opts: BindgenAttrs) -> Result<(), Diagnostic> {
        // Shortcut
        if opts.typescript_custom_section().is_none() {
            bail_span!(self, "#[wasm_bindgen] will not work on constants unless you are defining a #[wasm_bindgen(typescript_custom_section)].");
        }

        let typescript_custom_section = match get_expr(&self.expr) {
            syn::Expr::Lit(syn::ExprLit {
                lit: syn::Lit::Str(litstr),
                ..
            }) => ast::LitOrExpr::Lit(litstr.value()),
            expr => ast::LitOrExpr::Expr(expr.clone()),
        };

        program
            .typescript_custom_sections
            .push(typescript_custom_section);

        opts.check_used();

        Ok(())
    }
}

impl MacroParse<BindgenAttrs> for syn::ItemForeignMod {
    fn macro_parse(self, program: &mut ast::Program, opts: BindgenAttrs) -> Result<(), Diagnostic> {
        let mut errors = Vec::new();
        if let Some(other) = self.abi.name.filter(|l| l.value() != "C") {
            errors.push(err_span!(
                other,
                "only foreign mods with the `C` ABI are allowed"
            ));
        }
        let js_namespace = opts.js_namespace().map(|(s, _)| s);
        let module = module_from_opts(program, &opts)
            .map_err(|e| errors.push(e))
            .unwrap_or_default();
        for item in self.items.into_iter() {
            let ctx = ForeignItemCtx {
                module: module.clone(),
                js_namespace: js_namespace.clone(),
            };
            if let Err(e) = item.macro_parse(program, ctx) {
                errors.push(e);
            }
        }
        Diagnostic::from_vec(errors)?;
        opts.check_used();
        Ok(())
    }
}

struct ForeignItemCtx {
    module: Option<ast::ImportModule>,
    js_namespace: Option<JsNamespace>,
}

impl MacroParse<ForeignItemCtx> for syn::ForeignItem {
    fn macro_parse(
        mut self,
        program: &mut ast::Program,
        ctx: ForeignItemCtx,
    ) -> Result<(), Diagnostic> {
        let item_opts = {
            let attrs = match self {
                syn::ForeignItem::Fn(ref mut f) => &mut f.attrs,
                syn::ForeignItem::Type(ref mut t) => &mut t.attrs,
                syn::ForeignItem::Static(ref mut s) => &mut s.attrs,
                syn::ForeignItem::Verbatim(v) => {
                    let mut item: syn::ItemStatic =
                        syn::parse(v.into()).expect("only foreign functions/types allowed for now");
                    let item_opts = BindgenAttrs::find(&mut item.attrs)?;
                    let reexport = item_opts.reexport().cloned();
                    let kind = item.convert((program, item_opts, &ctx.module))?;

                    program.imports.push(ast::Import {
                        module: None,
                        js_namespace: None,
                        reexport,
                        kind,
                    });

                    return Ok(());
                }
                _ => panic!("only foreign functions/types allowed for now"),
            };
            BindgenAttrs::find(attrs)?
        };

        let js_namespace = item_opts
            .js_namespace()
            .map(|(s, _)| s)
            .or(ctx.js_namespace)
            .map(|s| s.0);
        let module = ctx.module;
        let reexport = item_opts.reexport().cloned();

        let kind = match self {
            syn::ForeignItem::Fn(f) => f.convert((program, item_opts, &module))?,
            syn::ForeignItem::Type(t) => t.convert((program, item_opts))?,
            syn::ForeignItem::Static(s) => s.convert((program, item_opts, &module))?,
            _ => panic!("only foreign functions/types allowed for now"),
        };

        // check for JS keywords

        // We only need to check if there isn't a JS namespace or module. If
        // there is namespace, then we already checked the namespace while
        // parsing. If there is a module, we can rename the import symbol to
        // avoid using keywords.
        let needs_check = js_namespace.is_none() && module.is_none();
        if needs_check {
            match &kind {
                ast::ImportKind::Function(import_function) => {
                    if matches!(import_function.kind, ast::ImportFunctionKind::Normal)
                        && is_non_value_js_keyword(&import_function.function.name)
                    {
                        bail_span!(
                            import_function.rust_name,
                            "Imported function cannot use the JS keyword `{}` as its name.",
                            import_function.function.name
                        );
                    }
                }
                ast::ImportKind::Static(import_static) => {
                    if is_non_value_js_keyword(&import_static.js_name) {
                        bail_span!(
                            import_static.rust_name,
                            "Imported static cannot use the JS keyword `{}` as its name.",
                            import_static.js_name
                        );
                    }
                }
                ast::ImportKind::String(_) => {
                    // static strings don't have JS names, so we don't need to check for JS keywords
                }
                ast::ImportKind::Type(import_type) => {
                    if is_non_value_js_keyword(&import_type.js_name) {
                        bail_span!(
                            import_type.rust_name,
                            "Imported type cannot use the JS keyword `{}` as its name.",
                            import_type.js_name
                        );
                    }
                }
                ast::ImportKind::Enum(_) => {
                    // string enums aren't possible here
                }
            }
        }

        program.imports.push(ast::Import {
            module,
            js_namespace,
            reexport,
            kind,
        });

        Ok(())
    }
}

pub fn module_from_opts(
    program: &mut ast::Program,
    opts: &BindgenAttrs,
) -> Result<Option<ast::ImportModule>, Diagnostic> {
    if let Some(path) = opts.wasm_bindgen() {
        program.wasm_bindgen = path.clone();
    }

    if let Some(path) = opts.js_sys() {
        program.js_sys = path.clone();
    }

    if let Some(path) = opts.wasm_bindgen_futures() {
        program.wasm_bindgen_futures = path.clone();
    }

    let mut errors = Vec::new();
    let module = if let Some((name, span)) = opts.module() {
        if opts.inline_js().is_some() {
            let msg = "cannot specify both `module` and `inline_js`";
            errors.push(Diagnostic::span_error(span, msg));
        }
        if opts.raw_module().is_some() {
            let msg = "cannot specify both `module` and `raw_module`";
            errors.push(Diagnostic::span_error(span, msg));
        }
        Some(ast::ImportModule::Named(name.to_string(), span))
    } else if let Some((name, span)) = opts.raw_module() {
        if opts.inline_js().is_some() {
            let msg = "cannot specify both `raw_module` and `inline_js`";
            errors.push(Diagnostic::span_error(span, msg));
        }
        Some(ast::ImportModule::RawNamed(name.to_string(), span))
    } else if let Some((js, _span)) = opts.inline_js() {
        let i = program.inline_js.len();
        program.inline_js.push(js.to_string());
        Some(ast::ImportModule::Inline(i))
    } else {
        None
    };
    Diagnostic::from_vec(errors)?;
    Ok(module)
}

/// Get the first type parameter of a generic type, errors on incorrect input.
fn extract_first_ty_param(ty: Option<&syn::Type>) -> Result<Option<syn::Type>, Diagnostic> {
    let t = match ty {
        Some(t) => t,
        None => return Ok(None),
    };
    let path = match *get_ty(t) {
        syn::Type::Path(syn::TypePath {
            qself: None,
            ref path,
        }) => path,
        _ => bail_span!(t, "must be Result<...>"),
    };
    let seg = path
        .segments
        .last()
        .ok_or_else(|| err_span!(t, "must have at least one segment"))?;
    let generics = match seg.arguments {
        syn::PathArguments::AngleBracketed(ref t) => t,
        _ => bail_span!(t, "must be Result<...>"),
    };
    let generic = generics
        .args
        .first()
        .ok_or_else(|| err_span!(t, "must have at least one generic parameter"))?;
    let ty = match generic {
        syn::GenericArgument::Type(t) => t,
        other => bail_span!(other, "must be a type parameter"),
    };
    match get_ty(ty) {
        syn::Type::Tuple(t) if t.elems.is_empty() => return Ok(None),
        _ => {}
    }
    Ok(Some(ty.clone()))
}

/// Extract the documentation comments from a Vec of attributes
fn extract_doc_comments(attrs: &[syn::Attribute]) -> Vec<String> {
    attrs
        .iter()
        .filter_map(|a| {
            // if the path segments include an ident of "doc" we know this
            // this is a doc comment
            if a.path().segments.iter().any(|s| s.ident == "doc") {
                let tokens = match &a.meta {
                    syn::Meta::Path(_) => None,
                    syn::Meta::List(list) => Some(list.tokens.clone()),
                    syn::Meta::NameValue(name_value) => Some(name_value.value.to_token_stream()),
                };

                Some(
                    // We want to filter out any Puncts so just grab the Literals
                    tokens.into_iter().flatten().filter_map(|t| match t {
                        TokenTree::Literal(lit) => {
                            let quoted = lit.to_string();
                            Some(try_unescape(&quoted).unwrap_or(quoted))
                        }
                        _ => None,
                    }),
                )
            } else {
                None
            }
        })
        //Fold up the [[String]] iter we created into Vec<String>
        .fold(vec![], |mut acc, a| {
            acc.extend(a);
            acc
        })
}

// Unescapes a quoted string. char::escape_debug() was used to escape the text.
fn try_unescape(mut s: &str) -> Option<String> {
    s = s.strip_prefix('"').unwrap_or(s);
    s = s.strip_suffix('"').unwrap_or(s);
    let mut result = String::with_capacity(s.len());
    let mut chars = s.chars();
    while let Some(c) = chars.next() {
        if c == '\\' {
            let c = chars.next()?;
            match c {
                't' => result.push('\t'),
                'r' => result.push('\r'),
                'n' => result.push('\n'),
                '\\' | '\'' | '"' => result.push(c),
                'u' => {
                    if chars.next() != Some('{') {
                        return None;
                    }
                    let (c, next) = unescape_unicode(&mut chars)?;
                    result.push(c);
                    if next != '}' {
                        return None;
                    }
                }
                _ => return None,
            }
        } else {
            result.push(c);
        }
    }
    Some(result)
}

fn unescape_unicode(chars: &mut Chars) -> Option<(char, char)> {
    let mut value = 0;
    for (i, c) in chars.enumerate() {
        match (i, c.to_digit(16)) {
            (0..=5, Some(num)) => value = (value << 4) | num,
            (1.., None) => return Some((char::from_u32(value)?, c)),
            _ => break,
        }
    }
    None
}

/// Extracts the last ident from the path
/// If generics is enabled, generics are validated
fn extract_path_ident(path: &syn::Path, allow_generics: bool) -> Result<Ident, Diagnostic> {
    for segment in path.segments.iter() {
        match &segment.arguments {
            syn::PathArguments::None => {}
            syn::PathArguments::AngleBracketed(_) => {
                if !allow_generics {
                    bail_span!(
                        path,
                        "paths with type parameters are not supported in this position"
                    )
                }
            }
            syn::PathArguments::Parenthesized(_) => {
                bail_span!(path, "parenthesized paths are not supported yet")
            }
        }
    }

    match path.segments.last() {
        Some(value) => Ok(value.ident.clone()),
        None => {
            bail_span!(path, "empty idents are not supported");
        }
    }
}

fn bail_generic_unsupported(span: impl Spanned + ToTokens) -> Result<(), Diagnostic> {
    bail_span!(span, "unsupported in wasm-bindgen generics");
}

fn validate_generic_type_param_bound(bound: &syn::TypeParamBound) -> Result<(), Diagnostic> {
    match bound {
        syn::TypeParamBound::Trait(trait_bound) => {
            // Higher-ranked trait bounds (for<'a>) are now supported
            if let syn::TraitBoundModifier::Maybe(question) = trait_bound.modifier {
                bail_generic_unsupported(question)?;
            }
        }
        syn::TypeParamBound::Lifetime(_) => {
            // Lifetime bounds (e.g., T: 'a) are now supported
        }
        syn::TypeParamBound::Verbatim(_) => {}
        _ => {}
    }
    Ok(())
}

/// Validates generic type parameters and their bounds both for inline parameters and where clauses.
/// Bails for const params. Lifetimes are supported via hoisting.
fn validate_generics(generics: &syn::Generics) -> Result<(), Diagnostic> {
    if let Some(where_clause) = &generics.where_clause {
        for predicate in &where_clause.predicates {
            match predicate {
                syn::WherePredicate::Type(predicate_type) => {
                    // Lifetime bounds on types (for<'a>) are now supported
                    predicate_type
                        .bounds
                        .iter()
                        .try_for_each(validate_generic_type_param_bound)?;
                }
                syn::WherePredicate::Lifetime(_) => {
                    // Lifetime bounds (e.g., 'a: 'b) are now supported
                }
                _ => bail_generic_unsupported(predicate)?,
            }
        }
    }

    for param in &generics.params {
        match param {
            syn::GenericParam::Lifetime(_) => {
                // Lifetimes are now supported via hoisting
            }
            syn::GenericParam::Type(type_param) => {
                type_param
                    .bounds
                    .iter()
                    .try_for_each(validate_generic_type_param_bound)?;
            }
            syn::GenericParam::Const(const_param) => bail_generic_unsupported(const_param)?,
        }
    }

    Ok(())
}

pub fn reset_attrs_used() {
    ATTRS.with(|state| {
        state.parsed.set(0);
        state.checks.set(0);
        state.unused_attrs.borrow_mut().clear();
    })
}

pub fn check_unused_attrs(tokens: &mut TokenStream) {
    ATTRS.with(|state| {
        assert_eq!(state.parsed.get(), state.checks.get());
        let unused_attrs = &*state.unused_attrs.borrow();
        if !unused_attrs.is_empty() {
            let unused_attrs = unused_attrs.iter().map(|UnusedState { error, ident }| {
                if *error {
                    let text = format!("invalid attribute {ident} in this position");
                    quote::quote_spanned! { ident.span() => ::core::compile_error!(#text); }
                } else {
                    quote::quote! { let #ident: (); }
                }
            });
            tokens.extend(quote::quote! {
                // Anonymous scope to prevent name clashes.
                const _: () = {
                    #(#unused_attrs)*
                };
            });
        }
    })
}

fn operation_kind(opts: &BindgenAttrs) -> ast::OperationKind {
    let mut operation_kind = ast::OperationKind::Regular;
    if opts.this().is_some() {
        operation_kind = ast::OperationKind::RegularThis;
    }
    if let Some(g) = opts.getter() {
        operation_kind = ast::OperationKind::Getter(g.clone());
    }
    if let Some(s) = opts.setter() {
        operation_kind = ast::OperationKind::Setter(s.clone());
    }
    if opts.indexing_getter().is_some() {
        operation_kind = ast::OperationKind::IndexingGetter;
    }
    if opts.indexing_setter().is_some() {
        operation_kind = ast::OperationKind::IndexingSetter;
    }
    if opts.indexing_deleter().is_some() {
        operation_kind = ast::OperationKind::IndexingDeleter;
    }
    operation_kind
}

pub fn link_to(opts: BindgenAttrs) -> Result<ast::LinkToModule, Diagnostic> {
    let mut program = ast::Program::default();
    let module = module_from_opts(&mut program, &opts)?.ok_or_else(|| {
        Diagnostic::span_error(Span::call_site(), "`link_to!` requires a module.")
    })?;
    if let ast::ImportModule::Named(p, s) | ast::ImportModule::RawNamed(p, s) = &module {
        if !p.starts_with("./") && !p.starts_with("../") && !p.starts_with('/') {
            return Err(Diagnostic::span_error(
                *s,
                "`link_to!` does not support module paths.",
            ));
        }
    }
    opts.enforce_used()?;
    program.linked_modules.push(module);
    Ok(ast::LinkToModule(program))
}

fn main(program: &ast::Program, mut f: ItemFn, tokens: &mut TokenStream) -> Result<(), Diagnostic> {
    if f.sig.ident != "main" {
        bail_span!(&f.sig.ident, "the main function has to be called main");
    }
    if let Some(constness) = f.sig.constness {
        bail_span!(&constness, "the main function cannot be const");
    }
    if !f.sig.generics.params.is_empty() {
        bail_span!(&f.sig.generics, "the main function cannot have generics");
    }
    if !f.sig.inputs.is_empty() {
        bail_span!(&f.sig.inputs, "the main function cannot have arguments");
    }

    let r#return = f.sig.output;
    f.sig.output = ReturnType::Default;
    let body = f.block.as_ref();

    let wasm_bindgen = &program.wasm_bindgen;
    let wasm_bindgen_futures = &program.wasm_bindgen_futures;

    if f.sig.asyncness.take().is_some() {
        *f.block = syn::parse2(quote::quote! {
                {
                    async fn __wasm_bindgen_generated_main() #r#return #body
                    #wasm_bindgen_futures::spawn_local(
                        async move {
                            use #wasm_bindgen::__rt::Main;
                            let __ret = __wasm_bindgen_generated_main();
                            (&mut &mut &mut #wasm_bindgen::__rt::MainWrapper(Some(__ret.await))).__wasm_bindgen_main()
                        },
                    )
                }
            })
            .unwrap();
    } else {
        *f.block = syn::parse2(quote::quote! {
            {
                fn __wasm_bindgen_generated_main() #r#return #body
                use #wasm_bindgen::__rt::Main;
                let __ret = __wasm_bindgen_generated_main();
                (&mut &mut &mut #wasm_bindgen::__rt::MainWrapper(Some(__ret))).__wasm_bindgen_main()
            }
        })
        .unwrap();
    }

    f.to_tokens(tokens);

    Ok(())
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_try_unescape() {
        use super::try_unescape;
        assert_eq!(try_unescape("hello").unwrap(), "hello");
        assert_eq!(try_unescape("\"hello").unwrap(), "hello");
        assert_eq!(try_unescape("hello\"").unwrap(), "hello");
        assert_eq!(try_unescape("\"hello\"").unwrap(), "hello");
        assert_eq!(try_unescape("hello\\\\").unwrap(), "hello\\");
        assert_eq!(try_unescape("hello\\n").unwrap(), "hello\n");
        assert_eq!(try_unescape("hello\\u"), None);
        assert_eq!(try_unescape("hello\\u{"), None);
        assert_eq!(try_unescape("hello\\u{}"), None);
        assert_eq!(try_unescape("hello\\u{0}").unwrap(), "hello\0");
        assert_eq!(try_unescape("hello\\u{000000}").unwrap(), "hello\0");
        assert_eq!(try_unescape("hello\\u{0000000}"), None);
    }
}
