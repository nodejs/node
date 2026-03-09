#![allow(clippy::redundant_allocation)]

use std::sync::RwLock;

use bytes_str::BytesStr;
use once_cell::sync::Lazy;
use rustc_hash::FxHashMap;
use serde::{Deserialize, Serialize};
use string_enum::StringEnum;
use swc_atoms::{
    atom,
    wtf8::{Wtf8, Wtf8Buf},
    Atom, Wtf8Atom,
};
use swc_common::{
    comments::{Comment, CommentKind, Comments},
    errors::HANDLER,
    sync::Lrc,
    util::take::Take,
    FileName, Mark, SourceMap, Span, Spanned, SyntaxContext, DUMMY_SP,
};
use swc_config::merge::Merge;
use swc_ecma_ast::*;
use swc_ecma_hooks::VisitMutHook;
use swc_ecma_parser::{parse_file_as_expr, Syntax};
use swc_ecma_utils::{
    drop_span, prepend_stmt, private_ident, quote_ident, str::is_line_terminator, ExprFactory,
    StmtLike,
};
use swc_ecma_visit::VisitMut;

use self::static_check::should_use_create_element;
use crate::refresh::options::{deserialize_refresh, RefreshOptions};

mod static_check;
#[cfg(test)]
mod tests;

/// https://babeljs.io/docs/en/babel-plugin-transform-react-jsx#runtime
#[derive(StringEnum, PartialEq, Eq, PartialOrd, Ord, Clone, Copy, Hash)]
pub enum Runtime {
    /// `automatic`
    Automatic,
    /// `classic`
    Classic,
    /// `preserve`
    Preserve,
}

/// Note: This will changed in v2
impl Default for Runtime {
    fn default() -> Self {
        Runtime::Classic
    }
}

#[derive(Debug, Default, Clone, Serialize, Deserialize, Eq, PartialEq, Merge)]
#[serde(rename_all = "camelCase")]
#[serde(deny_unknown_fields)]
pub struct Options {
    /// If this is `true`, swc will behave just like babel 8 with
    /// `BABEL_8_BREAKING: true`.
    #[serde(skip, default)]
    pub next: Option<bool>,

    #[serde(default)]
    pub runtime: Option<Runtime>,

    /// For automatic runtime
    #[serde(default)]
    pub import_source: Option<Atom>,

    #[serde(default)]
    pub pragma: Option<BytesStr>,
    #[serde(default)]
    pub pragma_frag: Option<BytesStr>,

    #[serde(default)]
    pub throw_if_namespace: Option<bool>,

    #[serde(default)]
    pub development: Option<bool>,

    // @babel/plugin-transform-react-jsx: Since "useBuiltIns" is removed in Babel 8, you can remove
    // it from the config.
    #[deprecated(
        since = "0.167.4",
        note = r#"Since `useBuiltIns` is removed in swc, you can remove it from the config."#
    )]
    #[serde(default, alias = "useBuiltIns")]
    pub use_builtins: Option<bool>,

    // '@babel/plugin-transform-react-jsx: Since Babel 8, an inline object with spread elements is
    // always used, and the "useSpread" option is no longer available. Please remove it from your
    // config.',
    #[deprecated(
        since = "0.167.4",
        note = r#"An inline object with spread elements is always used, and the `useSpread` option is no longer available. Please remove it from your config."#
    )]
    #[serde(default)]
    pub use_spread: Option<bool>,

    #[serde(default, deserialize_with = "deserialize_refresh")]
    // default to disabled since this is still considered as experimental by now
    pub refresh: Option<RefreshOptions>,
}

#[cfg(feature = "concurrent")]
macro_rules! static_str {
    ($s:expr) => {{
        static VAL: Lazy<BytesStr> = Lazy::new(|| $s.into());
        VAL.clone()
    }};
}

#[cfg(not(feature = "concurrent"))]
macro_rules! static_str {
    ($s:expr) => {
        $s.into()
    };
}

pub fn default_import_source() -> Atom {
    atom!("react")
}

pub fn default_pragma() -> BytesStr {
    static_str!("React.createElement")
}

pub fn default_pragma_frag() -> BytesStr {
    static_str!("React.Fragment")
}

fn default_throw_if_namespace() -> bool {
    true
}

/// Parse `src` to use as a `pragma` or `pragmaFrag` in jsx.
pub fn parse_expr_for_jsx(
    cm: &SourceMap,
    name: &str,
    src: BytesStr,
    top_level_mark: Mark,
) -> Box<Expr> {
    let fm = cm.new_source_file(cache_filename(name), src);

    parse_file_as_expr(
        &fm,
        Syntax::default(),
        Default::default(),
        None,
        &mut Vec::new(),
    )
    .map_err(|e| {
        if HANDLER.is_set() {
            HANDLER.with(|h| {
                e.into_diagnostic(h)
                    .note("Failed to parse jsx pragma")
                    .emit()
            })
        }
    })
    .map(drop_span)
    .map(|mut expr| {
        apply_mark(&mut expr, top_level_mark);
        expr
    })
    .unwrap_or_else(|()| {
        panic!(
            "failed to parse jsx option {}: '{}' is not an expression",
            name, fm.src,
        )
    })
}

fn apply_mark(e: &mut Expr, mark: Mark) {
    match e {
        Expr::Ident(i) => {
            i.ctxt = i.ctxt.apply_mark(mark);
        }
        Expr::Member(MemberExpr { obj, .. }) => {
            apply_mark(obj, mark);
        }
        _ => {}
    }
}

/// `@babel/plugin-transform-react-jsx`
///
/// Turn JSX into React function calls
///
///
/// `top_level_mark` should be [Mark] passed to
/// [swc_ecma_transforms_base::resolver::resolver_with_mark].
///
///
/// # Parameters
///
/// ## `top_level_ctxt`
///
/// This is used to reference `React` defined by the user.
///
/// e.g.
///
/// ```js
/// import React from 'react';
/// ```
pub fn hook<C>(
    cm: Lrc<SourceMap>,
    comments: Option<C>,
    options: Options,
    top_level_mark: Mark,
    unresolved_mark: Mark,
) -> impl VisitMutHook<()>
where
    C: Comments,
{
    Jsx {
        cm: cm.clone(),
        top_level_mark,
        unresolved_mark,
        runtime: options.runtime.unwrap_or_default(),
        import_source: options.import_source.unwrap_or_else(default_import_source),
        import_jsx: None,
        import_jsxs: None,
        import_fragment: None,
        import_create_element: None,

        pragma: Lrc::new(parse_expr_for_jsx(
            &cm,
            "pragma",
            options.pragma.unwrap_or_else(default_pragma),
            top_level_mark,
        )),
        comments,
        pragma_frag: Lrc::new(parse_expr_for_jsx(
            &cm,
            "pragmaFrag",
            options.pragma_frag.unwrap_or_else(default_pragma_frag),
            top_level_mark,
        )),
        development: options.development.unwrap_or_default(),
        throw_if_namespace: options
            .throw_if_namespace
            .unwrap_or_else(default_throw_if_namespace),
        top_level_node: true,
    }
}

// Re-export for compatibility
pub fn jsx<C>(
    cm: Lrc<SourceMap>,
    comments: Option<C>,
    options: Options,
    top_level_mark: Mark,
    unresolved_mark: Mark,
) -> impl Pass + VisitMut
where
    C: Comments,
{
    use swc_ecma_hooks::VisitMutWithHook;
    use swc_ecma_visit::visit_mut_pass;

    visit_mut_pass(VisitMutWithHook {
        hook: hook(cm, comments, options, top_level_mark, unresolved_mark),
        context: (),
    })
}

struct Jsx<C>
where
    C: Comments,
{
    cm: Lrc<SourceMap>,

    top_level_mark: Mark,
    unresolved_mark: Mark,

    runtime: Runtime,
    /// For automatic runtime.
    import_source: Atom,
    /// For automatic runtime.
    import_jsx: Option<Ident>,
    /// For automatic runtime.
    import_jsxs: Option<Ident>,
    /// For automatic runtime.
    import_create_element: Option<Ident>,
    /// For automatic runtime.
    import_fragment: Option<Ident>,
    top_level_node: bool,

    pragma: Lrc<Box<Expr>>,
    comments: Option<C>,
    pragma_frag: Lrc<Box<Expr>>,
    development: bool,
    throw_if_namespace: bool,
}

#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct JsxDirectives {
    pub runtime: Option<Runtime>,

    /// For automatic runtime.
    pub import_source: Option<Atom>,

    /// Parsed from `@jsx`
    pub pragma: Option<Lrc<Box<Expr>>>,

    /// Parsed from `@jsxFrag`
    pub pragma_frag: Option<Lrc<Box<Expr>>>,
}

impl JsxDirectives {
    pub fn from_comments(
        cm: &SourceMap,
        _: Span,
        comments: &[Comment],
        top_level_mark: Mark,
    ) -> Self {
        let mut res = JsxDirectives::default();

        for cmt in comments {
            if cmt.kind != CommentKind::Block {
                continue;
            }

            for line in cmt.text.lines() {
                let mut line = line.trim();
                if line.starts_with('*') {
                    line = line[1..].trim();
                }

                if !line.starts_with("@jsx") {
                    continue;
                }

                let mut words = line.split_whitespace();
                loop {
                    let pragma = words.next();
                    if pragma.is_none() {
                        break;
                    }
                    let val = words.next();

                    match pragma {
                        Some("@jsxRuntime") => match val {
                            Some("classic") => res.runtime = Some(Runtime::Classic),
                            Some("automatic") => res.runtime = Some(Runtime::Automatic),
                            None => {}
                            _ => {
                                if HANDLER.is_set() {
                                    HANDLER.with(|handler| {
                                        handler
                                            .struct_span_err(
                                                cmt.span,
                                                "Runtime must be either `classic` or `automatic`.",
                                            )
                                            .emit()
                                    });
                                }
                            }
                        },
                        Some("@jsxImportSource") => {
                            if let Some(src) = val {
                                res.runtime = Some(Runtime::Automatic);
                                res.import_source = Some(Atom::new(src));
                            }
                        }
                        Some("@jsxFrag") => {
                            if let Some(src) = val {
                                if is_valid_for_pragma(src) {
                                    // TODO: Optimize
                                    let mut e = parse_expr_for_jsx(
                                        cm,
                                        "module-jsx-pragma-frag",
                                        cache_source(src),
                                        top_level_mark,
                                    );
                                    e.set_span(cmt.span);
                                    res.pragma_frag = Some(e.into())
                                }
                            }
                        }
                        Some("@jsx") => {
                            if let Some(src) = val {
                                if is_valid_for_pragma(src) {
                                    // TODO: Optimize
                                    let mut e = parse_expr_for_jsx(
                                        cm,
                                        "module-jsx-pragma",
                                        cache_source(src),
                                        top_level_mark,
                                    );
                                    e.set_span(cmt.span);
                                    res.pragma = Some(e.into());
                                }
                            }
                        }
                        _ => {}
                    }
                }
            }
        }

        res
    }
}

#[cfg(feature = "concurrent")]
fn cache_filename(name: &str) -> Lrc<FileName> {
    static FILENAME_CACHE: Lazy<RwLock<FxHashMap<String, Lrc<FileName>>>> =
        Lazy::new(|| RwLock::new(FxHashMap::default()));

    {
        let cache = FILENAME_CACHE
            .read()
            .expect("Failed to read FILENAME_CACHE");
        if let Some(f) = cache.get(name) {
            return f.clone();
        }
    }

    let file = Lrc::new(FileName::Internal(format!("jsx-config-{name}.js")));

    {
        let mut cache = FILENAME_CACHE
            .write()
            .expect("Failed to write FILENAME_CACHE");
        cache.insert(name.to_string(), file.clone());
    }

    file
}

#[cfg(not(feature = "concurrent"))]
fn cache_filename(name: &str) -> Lrc<FileName> {
    Lrc::new(FileName::Internal(format!("jsx-config-{name}.js")))
}

#[cfg(feature = "concurrent")]
fn cache_source(src: &str) -> BytesStr {
    use rustc_hash::FxHashSet;

    static CACHE: Lazy<RwLock<FxHashSet<BytesStr>>> =
        Lazy::new(|| RwLock::new(FxHashSet::default()));

    {
        let cache = CACHE.write().unwrap();

        if let Some(cached) = cache.get(src) {
            return cached.clone();
        }
    }

    let cached: BytesStr = src.to_string().into();
    {
        let mut cache = CACHE.write().unwrap();
        cache.insert(cached.clone());
    }
    cached
}

#[cfg(not(feature = "concurrent"))]
fn cache_source(src: &str) -> BytesStr {
    // We cannot cache because Rc does not implement Send.
    src.to_string().into()
}

fn is_valid_for_pragma(s: &str) -> bool {
    if s.is_empty() {
        return false;
    }

    if !s.starts_with(|c: char| Ident::is_valid_start(c)) {
        return false;
    }

    for c in s.chars() {
        if !Ident::is_valid_continue(c) && c != '.' {
            return false;
        }
    }

    true
}

impl<C> Jsx<C>
where
    C: Comments,
{
    /// Process JSX attribute value, handling JSXElements and JSXFragments
    fn process_attr_value(&mut self, value: Option<JSXAttrValue>) -> Box<Expr> {
        match value {
            Some(JSXAttrValue::JSXElement(el)) => Box::new(self.jsx_elem_to_expr(*el)),
            Some(JSXAttrValue::JSXFragment(frag)) => Box::new(self.jsx_frag_to_expr(frag)),
            Some(JSXAttrValue::JSXExprContainer(container)) => match container.expr {
                JSXExpr::Expr(e) => e,
                JSXExpr::JSXEmptyExpr(_) => panic!("empty expression container"),
                #[cfg(swc_ast_unknown)]
                _ => panic!("unable to access unknown nodes"),
            },
            Some(v) => jsx_attr_value_to_expr(v).expect("empty expression container?"),
            None => true.into(),
        }
    }

    fn inject_runtime<T, F>(&mut self, body: &mut Vec<T>, inject: F)
    where
        T: StmtLike,
        // Fn(Vec<(local, imported)>, src, body)
        F: Fn(Vec<(Ident, IdentName)>, &str, &mut Vec<T>),
    {
        if self.runtime == Runtime::Automatic {
            if let Some(local) = self.import_create_element.take() {
                inject(
                    vec![(local, quote_ident!("createElement"))],
                    &self.import_source,
                    body,
                );
            }

            let imports = self.import_jsx.take();
            let imports = if self.development {
                imports
                    .map(|local| (local, quote_ident!("jsxDEV")))
                    .into_iter()
                    .chain(
                        self.import_fragment
                            .take()
                            .map(|local| (local, quote_ident!("Fragment"))),
                    )
                    .collect::<Vec<_>>()
            } else {
                imports
                    .map(|local| (local, quote_ident!("jsx")))
                    .into_iter()
                    .chain(
                        self.import_jsxs
                            .take()
                            .map(|local| (local, quote_ident!("jsxs"))),
                    )
                    .chain(
                        self.import_fragment
                            .take()
                            .map(|local| (local, quote_ident!("Fragment"))),
                    )
                    .collect::<Vec<_>>()
            };

            if !imports.is_empty() {
                let jsx_runtime = if self.development {
                    "jsx-dev-runtime"
                } else {
                    "jsx-runtime"
                };

                let value = format!("{}/{}", self.import_source, jsx_runtime);
                inject(imports, &value, body)
            }
        }
    }

    fn jsx_frag_to_expr(&mut self, el: JSXFragment) -> Expr {
        let mut span = el.span();

        if let Some(comments) = &self.comments {
            if span.lo.is_dummy() {
                span.lo = Span::dummy_with_cmt().lo;
            }

            comments.add_pure_comment(span.lo);
        }

        match self.runtime {
            Runtime::Automatic => {
                let fragment = self
                    .import_fragment
                    .get_or_insert_with(|| private_ident!("_Fragment"))
                    .clone();

                let mut props_obj = ObjectLit {
                    span: DUMMY_SP,
                    props: Vec::new(),
                };

                let children = el
                    .children
                    .into_iter()
                    .filter_map(|child| self.jsx_elem_child_to_expr(child))
                    .map(Some)
                    .collect::<Vec<_>>();

                let use_jsxs = match children.len() {
                    0 => false,
                    1 if matches!(children.first(), Some(Some(child)) if child.spread.is_none()) => {
                        props_obj
                            .props
                            .push(PropOrSpread::Prop(Box::new(Prop::KeyValue(KeyValueProp {
                                key: PropName::Ident(quote_ident!("children")),
                                value: children.into_iter().next().flatten().unwrap().expr,
                            }))));

                        false
                    }
                    _ => {
                        props_obj
                            .props
                            .push(PropOrSpread::Prop(Box::new(Prop::KeyValue(KeyValueProp {
                                key: PropName::Ident(quote_ident!("children")),
                                value: ArrayLit {
                                    span: DUMMY_SP,
                                    elems: children,
                                }
                                .into(),
                            }))));
                        true
                    }
                };

                let jsx = if use_jsxs && !self.development {
                    self.import_jsxs
                        .get_or_insert_with(|| private_ident!("_jsxs"))
                        .clone()
                } else {
                    let jsx = if self.development { "_jsxDEV" } else { "_jsx" };
                    self.import_jsx
                        .get_or_insert_with(|| private_ident!(jsx))
                        .clone()
                };

                // Build args Vec directly
                let args = if self.development {
                    vec![
                        fragment.as_arg(),
                        props_obj.as_arg(),
                        Expr::undefined(DUMMY_SP).as_arg(),
                        use_jsxs.as_arg(),
                    ]
                } else {
                    vec![fragment.as_arg(), props_obj.as_arg()]
                };

                CallExpr {
                    span,
                    callee: jsx.as_callee(),
                    args,
                    ..Default::default()
                }
                .into()
            }
            Runtime::Classic => {
                // Build args Vec directly for better performance
                let children_capacity = el.children.len();
                let mut args = Vec::with_capacity(2 + children_capacity);

                args.push((*self.pragma_frag).clone().as_arg());
                args.push(Lit::Null(Null { span: DUMMY_SP }).as_arg());

                // Add children
                for child in el.children {
                    if let Some(expr) = self.jsx_elem_child_to_expr(child) {
                        args.push(expr);
                    }
                }

                CallExpr {
                    span,
                    callee: (*self.pragma).clone().as_callee(),
                    args,
                    ..Default::default()
                }
                .into()
            }
            Runtime::Preserve => unreachable!(),
        }
    }

    /// # Automatic
    ///
    /// <div></div> => jsx('div', null);
    ///
    /// # Classic
    ///
    /// <div></div> => React.createElement('div', null);
    fn jsx_elem_to_expr(&mut self, el: JSXElement) -> Expr {
        let top_level_node = self.top_level_node;
        let mut span = el.span();
        let use_create_element = should_use_create_element(&el.opening.attrs);
        self.top_level_node = false;

        let name = self.jsx_name(el.opening.name);

        if let Some(comments) = &self.comments {
            if span.lo.is_dummy() {
                span.lo = Span::dummy_with_cmt().lo;
            }

            comments.add_pure_comment(span.lo);
        }

        match self.runtime {
            Runtime::Automatic => {
                // function jsx(tagName: string, props: { children: Node[], ... }, key: string)

                // Pre-allocate with estimated capacity
                let estimated_props_capacity = el.opening.attrs.len() + 1; // attrs + potential children
                let mut props_obj = ObjectLit {
                    span: DUMMY_SP,
                    props: Vec::with_capacity(estimated_props_capacity),
                };

                let mut key = None;
                let mut source_props = None;
                let mut self_props = None;

                for attr in el.opening.attrs {
                    match attr {
                        JSXAttrOrSpread::JSXAttr(attr) => {
                            //
                            match attr.name {
                                JSXAttrName::Ident(i) => {
                                    //
                                    if !use_create_element && i.sym == "key" {
                                        key = attr
                                            .value
                                            .and_then(jsx_attr_value_to_expr)
                                            .map(|expr| expr.as_arg());

                                        if key.is_none() && HANDLER.is_set() {
                                            HANDLER.with(|handler| {
                                                handler
                                                    .struct_span_err(
                                                        i.span,
                                                        "The value of property 'key' should not \
                                                         be empty",
                                                    )
                                                    .emit();
                                            });
                                        }
                                        continue;
                                    }

                                    if !use_create_element
                                        && *i.sym == *"__source"
                                        && self.development
                                    {
                                        if source_props.is_some() {
                                            panic!("Duplicate __source is found");
                                        }
                                        source_props = attr
                                            .value
                                            .and_then(jsx_attr_value_to_expr)
                                            .map(|expr| expr.as_arg());
                                        assert_ne!(
                                            source_props, None,
                                            "value of property '__source' should not be empty"
                                        );
                                        continue;
                                    }

                                    if !use_create_element
                                        && *i.sym == *"__self"
                                        && self.development
                                    {
                                        if self_props.is_some() {
                                            panic!("Duplicate __self is found");
                                        }
                                        self_props = attr
                                            .value
                                            .and_then(jsx_attr_value_to_expr)
                                            .map(|expr| expr.as_arg());
                                        assert_ne!(
                                            self_props, None,
                                            "value of property '__self' should not be empty"
                                        );
                                        continue;
                                    }

                                    let value = self.process_attr_value(attr.value);

                                    // TODO: Check if `i` is a valid identifier.
                                    let key = if i.sym.contains('-') {
                                        PropName::Str(Str {
                                            span: i.span,
                                            raw: None,
                                            value: i.sym.into(),
                                        })
                                    } else {
                                        PropName::Ident(i)
                                    };
                                    props_obj.props.push(PropOrSpread::Prop(Box::new(
                                        Prop::KeyValue(KeyValueProp { key, value }),
                                    )));
                                }
                                JSXAttrName::JSXNamespacedName(JSXNamespacedName {
                                    ns,
                                    name,
                                    ..
                                }) => {
                                    if self.throw_if_namespace && HANDLER.is_set() {
                                        HANDLER.with(|handler| {
                                            handler
                                                .struct_span_err(
                                                    span,
                                                    "JSX Namespace is disabled by default because \
                                                     react does not support it yet. You can \
                                                     specify jsc.transform.react.throwIfNamespace \
                                                     to false to override default behavior",
                                                )
                                                .emit()
                                        });
                                    }

                                    let value = self.process_attr_value(attr.value);

                                    let str_value = format!("{}:{}", ns.sym, name.sym);
                                    let key = Str {
                                        span,
                                        raw: None,
                                        value: str_value.into(),
                                    };
                                    let key = PropName::Str(key);

                                    props_obj.props.push(PropOrSpread::Prop(Box::new(
                                        Prop::KeyValue(KeyValueProp { key, value }),
                                    )));
                                }
                                #[cfg(swc_ast_unknown)]
                                _ => panic!("unable to access unknown nodes"),
                            }
                        }
                        JSXAttrOrSpread::SpreadElement(attr) => match *attr.expr {
                            Expr::Object(obj) => {
                                props_obj.props.extend(obj.props);
                            }
                            _ => {
                                props_obj.props.push(PropOrSpread::Spread(attr));
                            }
                        },
                        #[cfg(swc_ast_unknown)]
                        _ => panic!("unable to access unknown nodes"),
                    }
                }

                let mut children = el
                    .children
                    .into_iter()
                    .filter_map(|child| self.jsx_elem_child_to_expr(child))
                    .map(Some)
                    .collect::<Vec<_>>();

                let use_jsxs = match children.len() {
                    0 => false,
                    1 if matches!(children.first(), Some(Some(child)) if child.spread.is_none()) => {
                        if !use_create_element {
                            props_obj
                                .props
                                .push(PropOrSpread::Prop(Box::new(Prop::KeyValue(KeyValueProp {
                                    key: PropName::Ident(quote_ident!("children")),
                                    value: children
                                        .take()
                                        .into_iter()
                                        .next()
                                        .flatten()
                                        .unwrap()
                                        .expr,
                                }))));
                        }

                        false
                    }
                    _ => {
                        props_obj
                            .props
                            .push(PropOrSpread::Prop(Box::new(Prop::KeyValue(KeyValueProp {
                                key: PropName::Ident(quote_ident!("children")),
                                value: ArrayLit {
                                    span: DUMMY_SP,
                                    elems: children.take(),
                                }
                                .into(),
                            }))));
                        true
                    }
                };

                let jsx = if use_create_element {
                    self.import_create_element
                        .get_or_insert_with(|| private_ident!("_createElement"))
                        .clone()
                } else if use_jsxs && !self.development {
                    self.import_jsxs
                        .get_or_insert_with(|| private_ident!("_jsxs"))
                        .clone()
                } else {
                    let jsx = if self.development { "_jsxDEV" } else { "_jsx" };
                    self.import_jsx
                        .get_or_insert_with(|| private_ident!(jsx))
                        .clone()
                };

                self.top_level_node = top_level_node;

                // Build args Vec directly instead of using iterator chains
                let args = if use_create_element {
                    let mut args = Vec::with_capacity(2 + children.len());
                    args.push(name.as_arg());
                    args.push(props_obj.as_arg());
                    args.extend(children.into_iter().flatten());
                    args
                } else if self.development {
                    let mut args = Vec::with_capacity(6);
                    args.push(name.as_arg());
                    args.push(props_obj.as_arg());

                    // set undefined literal to key if key is None
                    let key = key.unwrap_or_else(|| Expr::undefined(DUMMY_SP).as_arg());
                    args.push(key);

                    args.push(use_jsxs.as_arg());

                    // set undefined literal to __source if __source is None
                    let source_props =
                        source_props.unwrap_or_else(|| Expr::undefined(DUMMY_SP).as_arg());
                    args.push(source_props);

                    // set undefined literal to __self if __self is None
                    let self_props =
                        self_props.unwrap_or_else(|| Expr::undefined(DUMMY_SP).as_arg());
                    args.push(self_props);

                    args
                } else {
                    let mut args = Vec::with_capacity(if key.is_some() { 3 } else { 2 });
                    args.push(name.as_arg());
                    args.push(props_obj.as_arg());
                    if let Some(key) = key {
                        args.push(key);
                    }
                    args
                };
                CallExpr {
                    span,
                    callee: jsx.as_callee(),
                    args,
                    ..Default::default()
                }
                .into()
            }
            Runtime::Classic => {
                // Build args Vec directly for better performance
                let children_capacity = el.children.len();
                let mut args = Vec::with_capacity(2 + children_capacity);

                args.push(name.as_arg());
                args.push(self.fold_attrs_for_classic(el.opening.attrs).as_arg());

                // Add children
                for child in el.children {
                    if let Some(expr) = self.jsx_elem_child_to_expr(child) {
                        args.push(expr);
                    }
                }

                CallExpr {
                    span,
                    callee: (*self.pragma).clone().as_callee(),
                    args,
                    ..Default::default()
                }
                .into()
            }
            Runtime::Preserve => unreachable!(),
        }
    }

    fn jsx_elem_child_to_expr(&mut self, c: JSXElementChild) -> Option<ExprOrSpread> {
        self.top_level_node = false;

        Some(match c {
            JSXElementChild::JSXText(text) => {
                // TODO(kdy1): Optimize
                let value = jsx_text_to_str_with_raw(&text.value, &text.raw);
                let s = Str {
                    span: text.span,
                    raw: None,
                    value,
                };

                if s.value.is_empty() {
                    return None;
                }

                Lit::Str(s).as_arg()
            }
            JSXElementChild::JSXExprContainer(JSXExprContainer {
                expr: JSXExpr::Expr(e),
                ..
            }) => e.as_arg(),
            JSXElementChild::JSXExprContainer(JSXExprContainer {
                expr: JSXExpr::JSXEmptyExpr(..),
                ..
            }) => return None,
            JSXElementChild::JSXElement(el) => self.jsx_elem_to_expr(*el).as_arg(),
            JSXElementChild::JSXFragment(el) => self.jsx_frag_to_expr(el).as_arg(),
            JSXElementChild::JSXSpreadChild(JSXSpreadChild { span, expr, .. }) => ExprOrSpread {
                spread: Some(span),
                expr,
            },
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        })
    }

    fn fold_attrs_for_classic(&mut self, attrs: Vec<JSXAttrOrSpread>) -> Box<Expr> {
        if attrs.is_empty() {
            return Lit::Null(Null { span: DUMMY_SP }).into();
        }
        let attr_cnt = attrs.len();

        let mut props = Vec::new();
        for attr in attrs {
            match attr {
                JSXAttrOrSpread::JSXAttr(attr) => {
                    props.push(PropOrSpread::Prop(Box::new(self.attr_to_prop(attr))))
                }
                JSXAttrOrSpread::SpreadElement(spread) => {
                    if attr_cnt == 1 {
                        return spread.expr;
                    }
                    // babel does some optimizations
                    match *spread.expr {
                        Expr::Object(obj) => props.extend(obj.props),
                        _ => props.push(PropOrSpread::Spread(spread)),
                    }
                }
                #[cfg(swc_ast_unknown)]
                _ => panic!("unable to access unknown nodes"),
            }
        }

        let obj = ObjectLit {
            span: DUMMY_SP,
            props,
        };

        obj.into()
    }

    fn attr_to_prop(&mut self, a: JSXAttr) -> Prop {
        let key = to_prop_name(a.name);
        let value = a
            .value
            .map(|v| match v {
                JSXAttrValue::Str(s) => {
                    let value = transform_jsx_attr_str(&s.value);

                    Lit::Str(Str {
                        span: s.span,
                        raw: None,
                        value: value.into(),
                    })
                    .into()
                }
                JSXAttrValue::JSXExprContainer(JSXExprContainer {
                    expr: JSXExpr::Expr(e),
                    ..
                }) => e,
                JSXAttrValue::JSXElement(element) => Box::new(self.jsx_elem_to_expr(*element)),
                JSXAttrValue::JSXFragment(fragment) => Box::new(self.jsx_frag_to_expr(fragment)),
                JSXAttrValue::JSXExprContainer(JSXExprContainer {
                    span: _,
                    expr: JSXExpr::JSXEmptyExpr(_),
                }) => unreachable!("attr_to_prop(JSXEmptyExpr)"),
                #[cfg(swc_ast_unknown)]
                _ => panic!("unable to access unknown nodes"),
            })
            .unwrap_or_else(|| {
                Lit::Bool(Bool {
                    span: key.span(),
                    value: true,
                })
                .into()
            });
        Prop::KeyValue(KeyValueProp { key, value })
    }
}

impl<C> Jsx<C>
where
    C: Comments,
{
    /// If we found required jsx directives, we returns true.
    fn parse_directives(&mut self, span: Span) -> bool {
        let mut found = false;

        let directives = self.comments.with_leading(span.lo, |comments| {
            JsxDirectives::from_comments(&self.cm, span, comments, self.top_level_mark)
        });

        let JsxDirectives {
            runtime,
            import_source,
            pragma,
            pragma_frag,
        } = directives;

        if let Some(runtime) = runtime {
            found = true;
            self.runtime = runtime;
        }

        if let Some(import_source) = import_source {
            found = true;
            self.import_source = import_source;
        }

        if let Some(pragma) = pragma {
            if let Runtime::Automatic = self.runtime {
                if HANDLER.is_set() {
                    HANDLER.with(|handler| {
                        handler
                            .struct_span_err(
                                pragma.span(),
                                "pragma cannot be set when runtime is automatic",
                            )
                            .emit()
                    });
                }
            }

            found = true;
            self.pragma = pragma;
        }

        if let Some(pragma_frag) = pragma_frag {
            if let Runtime::Automatic = self.runtime {
                if HANDLER.is_set() {
                    HANDLER.with(|handler| {
                        handler
                            .struct_span_err(
                                pragma_frag.span(),
                                "pragmaFrag cannot be set when runtime is automatic",
                            )
                            .emit()
                    });
                }
            }

            found = true;
            self.pragma_frag = pragma_frag;
        }

        found
    }
}

impl<C> VisitMutHook<()> for Jsx<C>
where
    C: Comments,
{
    /// Called after visiting children of an expression.
    ///
    /// This is where we transform JSX syntax to JavaScript function calls.
    /// By doing this in exit_expr (after children are visited), we ensure that
    /// jsx_src and jsx_self have already added their __source and __self
    /// attributes.
    fn exit_expr(&mut self, expr: &mut Expr, _ctx: &mut ()) {
        let top_level_node = self.top_level_node;
        let mut did_work = false;

        if let Expr::JSXElement(el) = expr {
            did_work = true;
            // <div></div> => React.createElement('div', null);
            *expr = self.jsx_elem_to_expr(*el.take());
        } else if let Expr::JSXFragment(frag) = expr {
            // <></> => React.createElement(React.Fragment, null);
            did_work = true;
            *expr = self.jsx_frag_to_expr(frag.take());
        } else if let Expr::Paren(ParenExpr {
            expr: inner_expr, ..
        }) = expr
        {
            if let Expr::JSXElement(el) = &mut **inner_expr {
                did_work = true;
                *expr = self.jsx_elem_to_expr(*el.take());
            } else if let Expr::JSXFragment(frag) = &mut **inner_expr {
                // <></> => React.createElement(React.Fragment, null);
                did_work = true;
                *expr = self.jsx_frag_to_expr(frag.take());
            }
        }

        if did_work {
            self.top_level_node = false;
        }

        self.top_level_node = top_level_node;
    }

    fn enter_module(&mut self, module: &mut Module, _ctx: &mut ()) {
        self.parse_directives(module.span);

        for item in &module.body {
            let span = item.span();
            if self.parse_directives(span) {
                break;
            }
        }
    }

    fn exit_module(&mut self, module: &mut Module, _ctx: &mut ()) {
        if self.runtime == Runtime::Automatic {
            self.inject_runtime(&mut module.body, |imports, src, stmts| {
                let specifiers = imports
                    .into_iter()
                    .map(|(local, imported)| {
                        ImportSpecifier::Named(ImportNamedSpecifier {
                            span: DUMMY_SP,
                            local,
                            imported: Some(ModuleExportName::Ident(imported.into())),
                            is_type_only: false,
                        })
                    })
                    .collect();

                prepend_stmt(
                    stmts,
                    ImportDecl {
                        span: DUMMY_SP,
                        specifiers,
                        src: Str {
                            span: DUMMY_SP,
                            raw: None,
                            value: src.into(),
                        }
                        .into(),
                        type_only: Default::default(),
                        with: Default::default(),
                        phase: Default::default(),
                    }
                    .into(),
                )
            });
        }
    }

    fn enter_script(&mut self, script: &mut Script, _ctx: &mut ()) {
        self.parse_directives(script.span);

        for item in &script.body {
            let span = item.span();
            if self.parse_directives(span) {
                break;
            }
        }
    }

    fn exit_script(&mut self, script: &mut Script, _ctx: &mut ()) {
        if self.runtime == Runtime::Automatic {
            let mark = self.unresolved_mark;
            self.inject_runtime(&mut script.body, |imports, src, stmts| {
                prepend_stmt(stmts, add_require(imports, src, mark))
            });
        }
    }
}

// const { createElement } = require('react')
// const { jsx: jsx } = require('react/jsx-runtime')
fn add_require(imports: Vec<(Ident, IdentName)>, src: &str, unresolved_mark: Mark) -> Stmt {
    VarDecl {
        span: DUMMY_SP,
        kind: VarDeclKind::Const,
        declare: false,
        decls: vec![VarDeclarator {
            span: DUMMY_SP,
            name: Pat::Object(ObjectPat {
                span: DUMMY_SP,
                props: imports
                    .into_iter()
                    .map(|(local, imported)| {
                        if imported.sym != local.sym {
                            ObjectPatProp::KeyValue(KeyValuePatProp {
                                key: PropName::Ident(imported),
                                value: Box::new(Pat::Ident(local.into())),
                            })
                        } else {
                            ObjectPatProp::Assign(AssignPatProp {
                                span: DUMMY_SP,
                                key: local.into(),
                                value: None,
                            })
                        }
                    })
                    .collect(),
                optional: false,
                type_ann: None,
            }),
            // require('react')
            init: Some(Box::new(Expr::Call(CallExpr {
                span: DUMMY_SP,
                callee: Callee::Expr(Box::new(Expr::Ident(Ident {
                    ctxt: SyntaxContext::empty().apply_mark(unresolved_mark),
                    sym: atom!("require"),
                    optional: false,
                    ..Default::default()
                }))),
                args: vec![ExprOrSpread {
                    spread: None,
                    expr: Box::new(Expr::Lit(Lit::Str(Str {
                        span: DUMMY_SP,
                        value: src.into(),
                        raw: None,
                    }))),
                }],
                ..Default::default()
            }))),
            definite: false,
        }],
        ..Default::default()
    }
    .into()
}

impl<C> Jsx<C>
where
    C: Comments,
{
    fn jsx_name(&self, name: JSXElementName) -> Box<Expr> {
        let span = name.span();
        match name {
            JSXElementName::Ident(i) => {
                if i.sym == "this" {
                    return ThisExpr { span }.into();
                }

                // If it starts with lowercase
                if i.as_ref().starts_with(|c: char| c.is_ascii_lowercase()) {
                    Lit::Str(Str {
                        span,
                        raw: None,
                        value: i.sym.into(),
                    })
                    .into()
                } else {
                    i.into()
                }
            }
            JSXElementName::JSXNamespacedName(JSXNamespacedName {
                ref ns, ref name, ..
            }) => {
                if self.throw_if_namespace && HANDLER.is_set() {
                    HANDLER.with(|handler| {
                        handler
                            .struct_span_err(
                                span,
                                "JSX Namespace is disabled by default because react does not \
                                 support it yet. You can specify \
                                 jsc.transform.react.throwIfNamespace to false to override \
                                 default behavior",
                            )
                            .emit()
                    });
                }

                let value = format!("{}:{}", ns.sym, name.sym);

                Lit::Str(Str {
                    span,
                    raw: None,
                    value: value.into(),
                })
                .into()
            }
            JSXElementName::JSXMemberExpr(JSXMemberExpr { obj, prop, .. }) => {
                fn convert_obj(obj: JSXObject) -> Box<Expr> {
                    let span = obj.span();

                    (match obj {
                        JSXObject::Ident(i) => {
                            if i.sym == "this" {
                                Expr::This(ThisExpr { span })
                            } else {
                                i.into()
                            }
                        }
                        JSXObject::JSXMemberExpr(e) => MemberExpr {
                            span,
                            obj: convert_obj(e.obj),
                            prop: MemberProp::Ident(e.prop),
                        }
                        .into(),
                        #[cfg(swc_ast_unknown)]
                        _ => panic!("unable to access unknown nodes"),
                    })
                    .into()
                }
                MemberExpr {
                    span,
                    obj: convert_obj(obj),
                    prop: MemberProp::Ident(prop),
                }
                .into()
            }
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        }
    }
}

fn to_prop_name(n: JSXAttrName) -> PropName {
    let span = n.span();

    match n {
        JSXAttrName::Ident(i) => {
            if i.sym.contains('-') {
                PropName::Str(Str {
                    span,
                    raw: None,
                    value: i.sym.into(),
                })
            } else {
                PropName::Ident(i)
            }
        }
        JSXAttrName::JSXNamespacedName(JSXNamespacedName { ns, name, .. }) => {
            let value = format!("{}:{}", ns.sym, name.sym);

            PropName::Str(Str {
                span,
                raw: None,
                value: value.into(),
            })
        }
        #[cfg(swc_ast_unknown)]
        _ => panic!("unable to access unknown nodes"),
    }
}

/// https://github.com/microsoft/TypeScript/blob/9e20e032effad965567d4a1e1c30d5433b0a3332/src/compiler/transformers/jsx.ts#L572-L608
///
/// JSX trims whitespace at the end and beginning of lines, except that the
/// start/end of a tag is considered a start/end of a line only if that line is
/// on the same line as the closing tag. See examples in
/// tests/cases/conformance/jsx/tsxReactEmitWhitespace.tsx
/// See also https://www.w3.org/TR/html4/struct/text.html#h-9.1 and https://www.w3.org/TR/CSS2/text.html#white-space-model
///
/// An equivalent algorithm would be:
/// - If there is only one line, return it.
/// - If there is only whitespace (but multiple lines), return `undefined`.
/// - Split the text into lines.
/// - 'trimRight' the first line, 'trimLeft' the last line, 'trim' middle lines.
/// - Decode entities on each line (individually).
/// - Remove empty lines and join the rest with " ".
///
/// This version takes both `value` (decoded) and `raw` (original source) to
/// preserve whitespace that was explicitly encoded as HTML entities like
/// `&#32;`, `&#9;`, `&#10;`, `&#13;`.
#[inline]
fn jsx_text_to_str_with_raw(value: &Atom, raw: &Atom) -> Wtf8Atom {
    // Fast path: if no HTML entities (raw == value), use the simple algorithm
    if value.as_str() == raw.as_str() {
        return jsx_text_to_str_impl(value).into();
    }

    // Build a mask of which byte positions in value came from HTML entities
    let entity_mask = build_entity_mask(value, raw);

    jsx_text_to_str_with_entity_mask(value, &entity_mask).into()
}

/// Build a mask indicating which character positions in `value` came from HTML
/// entities in `raw`.
///
/// Returns a Vec<bool> where true means the character at that index was from an
/// entity.
fn build_entity_mask(value: &str, raw: &str) -> Vec<bool> {
    let mut mask = vec![false; value.chars().count()];
    let mut value_char_idx = 0;
    let mut raw_chars = raw.chars().peekable();

    while let Some(raw_c) = raw_chars.next() {
        if raw_c == '&' {
            // Possible HTML entity
            let mut entity_chars: Vec<char> = vec!['&'];
            let mut found_semicolon = false;

            // Collect up to 10 characters to match entity pattern
            for _ in 0..10 {
                if let Some(&next_c) = raw_chars.peek() {
                    entity_chars.push(next_c);
                    raw_chars.next();
                    if next_c == ';' {
                        found_semicolon = true;
                        break;
                    }
                } else {
                    break;
                }
            }

            if found_semicolon && is_valid_entity(&entity_chars) {
                // This was a valid entity - mark this position in value as from
                // entity
                if value_char_idx < mask.len() {
                    mask[value_char_idx] = true;
                }
                value_char_idx += 1;
            } else {
                // Not a valid entity, the '&' is literal
                value_char_idx += 1;
                // The other characters we consumed are also literal
                for _ in 1..entity_chars.len() {
                    value_char_idx += 1;
                }
            }
        } else {
            // Regular character
            value_char_idx += 1;
        }
    }

    mask
}

/// Check if the collected characters form a valid HTML entity
fn is_valid_entity(chars: &[char]) -> bool {
    if chars.len() < 3 {
        return false;
    }
    if chars[0] != '&' || chars[chars.len() - 1] != ';' {
        return false;
    }

    let inner: String = chars[1..chars.len() - 1].iter().collect();

    if let Some(stripped) = inner.strip_prefix('#') {
        // Numeric entity
        if let Some(hex) = stripped
            .strip_prefix('x')
            .or_else(|| stripped.strip_prefix('X'))
        {
            // Hex: &#xHHHH;
            !hex.is_empty() && hex.chars().all(|c| c.is_ascii_hexdigit())
        } else {
            // Decimal: &#DDDD;
            !stripped.is_empty() && stripped.chars().all(|c| c.is_ascii_digit())
        }
    } else {
        // Named entity - check against known entities
        is_known_html_entity(&inner)
    }
}

/// Check if name is a known HTML entity
fn is_known_html_entity(name: &str) -> bool {
    // Common HTML entities that decode to whitespace or other characters
    // This list matches the entities defined in swc_ecma_lexer jsx.rs xhtml!
    // macro
    matches!(
        name,
        "nbsp"
            | "iexcl"
            | "cent"
            | "pound"
            | "curren"
            | "yen"
            | "brvbar"
            | "sect"
            | "uml"
            | "copy"
            | "ordf"
            | "laquo"
            | "not"
            | "shy"
            | "reg"
            | "macr"
            | "deg"
            | "plusmn"
            | "sup2"
            | "sup3"
            | "acute"
            | "micro"
            | "para"
            | "middot"
            | "cedil"
            | "sup1"
            | "ordm"
            | "raquo"
            | "frac14"
            | "frac12"
            | "frac34"
            | "iquest"
            | "Agrave"
            | "Aacute"
            | "Acirc"
            | "Atilde"
            | "Auml"
            | "Aring"
            | "AElig"
            | "Ccedil"
            | "Egrave"
            | "Eacute"
            | "Ecirc"
            | "Euml"
            | "Igrave"
            | "Iacute"
            | "Icirc"
            | "Iuml"
            | "ETH"
            | "Ntilde"
            | "Ograve"
            | "Oacute"
            | "Ocirc"
            | "Otilde"
            | "Ouml"
            | "times"
            | "Oslash"
            | "Ugrave"
            | "Uacute"
            | "Ucirc"
            | "Uuml"
            | "Yacute"
            | "THORN"
            | "szlig"
            | "agrave"
            | "aacute"
            | "acirc"
            | "atilde"
            | "auml"
            | "aring"
            | "aelig"
            | "ccedil"
            | "egrave"
            | "eacute"
            | "ecirc"
            | "euml"
            | "igrave"
            | "iacute"
            | "icirc"
            | "iuml"
            | "eth"
            | "ntilde"
            | "ograve"
            | "oacute"
            | "ocirc"
            | "otilde"
            | "ouml"
            | "divide"
            | "oslash"
            | "ugrave"
            | "uacute"
            | "ucirc"
            | "uuml"
            | "yacute"
            | "thorn"
            | "yuml"
            | "OElig"
            | "oelig"
            | "Scaron"
            | "scaron"
            | "Yuml"
            | "fnof"
            | "circ"
            | "tilde"
            | "Alpha"
            | "Beta"
            | "Gamma"
            | "Delta"
            | "Epsilon"
            | "Zeta"
            | "Eta"
            | "Theta"
            | "Iota"
            | "Kappa"
            | "Lambda"
            | "Mu"
            | "Nu"
            | "Xi"
            | "Omicron"
            | "Pi"
            | "Rho"
            | "Sigma"
            | "Tau"
            | "Upsilon"
            | "Phi"
            | "Chi"
            | "Psi"
            | "Omega"
            | "alpha"
            | "beta"
            | "gamma"
            | "delta"
            | "epsilon"
            | "zeta"
            | "eta"
            | "theta"
            | "iota"
            | "kappa"
            | "lambda"
            | "mu"
            | "nu"
            | "xi"
            | "omicron"
            | "pi"
            | "rho"
            | "sigmaf"
            | "sigma"
            | "tau"
            | "upsilon"
            | "phi"
            | "chi"
            | "psi"
            | "omega"
            | "thetasym"
            | "upsih"
            | "piv"
            | "ensp"
            | "emsp"
            | "thinsp"
            | "zwnj"
            | "zwj"
            | "lrm"
            | "rlm"
            | "ndash"
            | "mdash"
            | "lsquo"
            | "rsquo"
            | "sbquo"
            | "ldquo"
            | "rdquo"
            | "bdquo"
            | "dagger"
            | "Dagger"
            | "bull"
            | "hellip"
            | "permil"
            | "prime"
            | "Prime"
            | "lsaquo"
            | "rsaquo"
            | "oline"
            | "frasl"
            | "euro"
            | "image"
            | "weierp"
            | "real"
            | "trade"
            | "alefsym"
            | "larr"
            | "uarr"
            | "rarr"
            | "darr"
            | "harr"
            | "crarr"
            | "lArr"
            | "uArr"
            | "rArr"
            | "dArr"
            | "hArr"
            | "forall"
            | "part"
            | "exist"
            | "empty"
            | "nabla"
            | "isin"
            | "notin"
            | "ni"
            | "prod"
            | "sum"
            | "minus"
            | "lowast"
            | "radic"
            | "prop"
            | "infin"
            | "ang"
            | "and"
            | "or"
            | "cap"
            | "cup"
            | "int"
            | "there4"
            | "sim"
            | "cong"
            | "asymp"
            | "ne"
            | "equiv"
            | "le"
            | "ge"
            | "sub"
            | "sup"
            | "nsub"
            | "sube"
            | "supe"
            | "oplus"
            | "otimes"
            | "perp"
            | "sdot"
            | "lceil"
            | "rceil"
            | "lfloor"
            | "rfloor"
            | "lang"
            | "rang"
            | "loz"
            | "spades"
            | "clubs"
            | "hearts"
            | "diams"
            | "quot"
            | "amp"
            | "lt"
            | "gt"
    )
}

/// JSX text processing with entity mask - preserves whitespace from HTML
/// entities
fn jsx_text_to_str_with_entity_mask(t: &str, entity_mask: &[bool]) -> Atom {
    // Fast path: if no line terminators and no trimmable whitespace
    // (whitespace that's not from entities at the leading edge)
    let chars: Vec<char> = t.chars().collect();
    let has_line_terminator = chars.iter().any(|&c| is_line_terminator(c));

    let has_trimmable_leading = chars
        .first()
        .is_some_and(|&c| is_white_space_single_line(c) && !entity_mask.first().unwrap_or(&false));

    // For single-line text, we keep trailing whitespace (matching original
    // behavior)
    if !t.is_empty() && !has_line_terminator && !has_trimmable_leading {
        return t.into();
    }

    let mut acc: Option<String> = None;
    let mut only_line: Option<String> = None;
    let mut line_start: Option<usize> = Some(0);
    let mut line_end: Option<usize> = None;

    for (char_idx, c) in chars.iter().enumerate() {
        let is_from_entity = *entity_mask.get(char_idx).unwrap_or(&false);

        if is_line_terminator(*c) {
            // Process current line - trim both leading AND trailing (intermediate
            // line)
            if let (Some(start), Some(end)) = (line_start, line_end) {
                let line_text = extract_line_content(&chars, start, end, entity_mask, true, true);
                add_line_of_jsx_text_owned(line_text, &mut acc, &mut only_line);
            }
            line_start = None;
            line_end = None;
        } else if !is_white_space_single_line(*c) || is_from_entity {
            // Non-whitespace or entity-derived whitespace - counts as content
            line_end = Some(char_idx + 1);
            if line_start.is_none() {
                line_start = Some(char_idx);
            }
        }
    }

    // Handle final line - only trim leading, keep trailing (matching original
    // behavior)
    if let Some(start) = line_start {
        // For final line, take from first non-whitespace (or entity) to end of string
        let line_text = extract_line_content(&chars, start, chars.len(), entity_mask, true, false);
        add_line_of_jsx_text_owned(line_text, &mut acc, &mut only_line);
    }

    if let Some(acc) = acc {
        acc.into()
    } else if let Some(only_line) = only_line {
        only_line.into()
    } else {
        "".into()
    }
}

/// Extract line content, optionally trimming non-entity whitespace from edges
///
/// - `trim_leading`: if true, trim leading non-entity whitespace
/// - `trim_trailing`: if true, trim trailing non-entity whitespace
fn extract_line_content(
    chars: &[char],
    start: usize,
    end: usize,
    entity_mask: &[bool],
    trim_leading: bool,
    trim_trailing: bool,
) -> String {
    // Find first non-trimmable position (if trim_leading is true)
    let mut actual_start = start;
    if trim_leading {
        while actual_start < end {
            let c = chars[actual_start];
            let is_from_entity = *entity_mask.get(actual_start).unwrap_or(&false);
            if !is_white_space_single_line(c) || is_from_entity {
                break;
            }
            actual_start += 1;
        }
    }

    // Find last non-trimmable position (if trim_trailing is true)
    let mut actual_end = end;
    if trim_trailing {
        while actual_end > actual_start {
            let c = chars[actual_end - 1];
            let is_from_entity = *entity_mask.get(actual_end - 1).unwrap_or(&false);
            if !is_white_space_single_line(c) || is_from_entity {
                break;
            }
            actual_end -= 1;
        }
    }

    chars[actual_start..actual_end].iter().collect()
}

/// Owned version of add_line_of_jsx_text for use with entity mask processing
fn add_line_of_jsx_text_owned(
    line: String,
    acc: &mut Option<String>,
    only_line: &mut Option<String>,
) {
    if line.is_empty() {
        return;
    }

    if let Some(buffer) = acc.as_mut() {
        buffer.push(' ');
        buffer.push_str(&line);
    } else if let Some(only_line_content) = only_line.take() {
        let mut buffer = String::with_capacity(line.len() * 2);
        buffer.push_str(&only_line_content);
        buffer.push(' ');
        buffer.push_str(&line);
        *acc = Some(buffer);
    } else {
        *only_line = Some(line);
    }
}

#[allow(dead_code)]
#[inline]
fn jsx_text_to_str<'a, T>(t: &'a T) -> Wtf8Atom
where
    &'a T: Into<&'a Wtf8>,
    T: ?Sized,
{
    let t = t.into();
    // Fast path: JSX text is almost always valid UTF-8
    if let Some(s) = t.as_str() {
        return jsx_text_to_str_impl(s).into();
    }

    // Slow path: Handle Wtf8 with surrogates (extremely rare)
    jsx_text_to_str_wtf8_impl(t)
}

/// Handle JSX text with surrogates
fn jsx_text_to_str_wtf8_impl(t: &Wtf8) -> Wtf8Atom {
    let mut acc: Option<Wtf8Buf> = None;
    let mut only_line: Option<(usize, usize)> = None; // (start, end) byte positions
    let mut first_non_whitespace: Option<usize> = Some(0);
    let mut last_non_whitespace: Option<usize> = None;

    let mut byte_pos = 0;
    for cp in t.code_points() {
        let c = cp.to_char_lossy();
        let cp_value = cp.to_u32();

        // Calculate byte length of this code point in WTF-8
        let cp_byte_len = if cp_value < 0x80 {
            1
        } else if cp_value < 0x800 {
            2
        } else if cp_value < 0x10000 {
            3
        } else {
            4
        };

        if is_line_terminator(c) {
            if let (Some(first), Some(last)) = (first_non_whitespace, last_non_whitespace) {
                add_line_of_jsx_text_wtf8(first, last, t, &mut acc, &mut only_line);
            }
            first_non_whitespace = None;
        } else if !is_white_space_single_line(c) {
            last_non_whitespace = Some(byte_pos + cp_byte_len);
            if first_non_whitespace.is_none() {
                first_non_whitespace.replace(byte_pos);
            }
        }

        byte_pos += cp_byte_len;
    }

    // Handle final line
    if let Some(first) = first_non_whitespace {
        add_line_of_jsx_text_wtf8(first, t.len(), t, &mut acc, &mut only_line);
    }

    if let Some(acc) = acc {
        acc.into()
    } else if let Some((start, end)) = only_line {
        t.slice(start, end).into()
    } else {
        Wtf8Atom::default()
    }
}

/// Helper for adding lines of JSX text when handling Wtf8 with surrogates
fn add_line_of_jsx_text_wtf8(
    line_start: usize,
    line_end: usize,
    source: &Wtf8,
    acc: &mut Option<Wtf8Buf>,
    only_line: &mut Option<(usize, usize)>,
) {
    if let Some((only_start, only_end)) = only_line.take() {
        // Second line - create accumulator
        let mut buffer = Wtf8Buf::with_capacity(source.len());
        buffer.push_wtf8(source.slice(only_start, only_end));
        buffer.push_str(" ");
        buffer.push_wtf8(source.slice(line_start, line_end));
        *acc = Some(buffer);
    } else if let Some(ref mut buffer) = acc {
        // Subsequent lines
        buffer.push_str(" ");
        buffer.push_wtf8(source.slice(line_start, line_end));
    } else {
        // First line
        *only_line = Some((line_start, line_end));
    }
}

/// Internal implementation that works with &str
#[inline]
fn jsx_text_to_str_impl(t: &str) -> Atom {
    // Fast path: if no line terminators and no leading/trailing whitespace
    if !t.is_empty()
        && !t.chars().any(is_line_terminator)
        && !t.starts_with(is_white_space_single_line)
        && !t.ends_with(is_white_space_single_line)
    {
        return t.into();
    }

    let mut acc: Option<String> = None;
    let mut only_line: Option<&str> = None;
    let mut first_non_whitespace: Option<usize> = Some(0);
    let mut last_non_whitespace: Option<usize> = None;

    for (index, c) in t.char_indices() {
        if is_line_terminator(c) {
            if let (Some(first), Some(last)) = (first_non_whitespace, last_non_whitespace) {
                let line_text = &t[first..last];
                add_line_of_jsx_text(line_text, &mut acc, &mut only_line);
            }
            first_non_whitespace = None;
        } else if !is_white_space_single_line(c) {
            last_non_whitespace = Some(index + c.len_utf8());
            if first_non_whitespace.is_none() {
                first_non_whitespace.replace(index);
            }
        }
    }

    if let Some(first) = first_non_whitespace {
        let line_text = &t[first..];
        add_line_of_jsx_text(line_text, &mut acc, &mut only_line);
    }

    if let Some(acc) = acc {
        acc.into()
    } else if let Some(only_line) = only_line {
        only_line.into()
    } else {
        "".into()
    }
}

/// [TODO]: Re-validate this whitespace handling logic.
///
/// We cannot use [swc_ecma_utils::str::is_white_space_single_line] because
/// HTML entities (like `&nbsp;` → `\u{00a0}`) are pre-processed by the parser,
/// making it impossible to distinguish them from literal Unicode characters. We
/// should never trim HTML entities.
///
/// As a reference, Babel only trims regular spaces and tabs, so this is a
/// simplified implementation already in use.
/// https://github.com/babel/babel/blob/e5c8dc7330cb2f66c37637677609df90b31ff0de/packages/babel-types/src/utils/react/cleanJSXElementLiteralChild.ts#L28-L39
fn is_white_space_single_line(c: char) -> bool {
    matches!(c, ' ' | '\t')
}

// less allocations trick from OXC
// https://github.com/oxc-project/oxc/blob/4c35f4abb6874bd741b84b34df7889637425e9ea/crates/oxc_transformer/src/jsx/jsx_impl.rs#L1061-L1091
fn add_line_of_jsx_text<'a>(
    trimmed_line: &'a str,
    acc: &mut Option<String>,
    only_line: &mut Option<&'a str>,
) {
    if let Some(buffer) = acc.as_mut() {
        // Already some text in accumulator. Push a space before this line is added to
        // `acc`.
        buffer.push(' ');
    } else if let Some(only_line_content) = only_line.take() {
        // This is the 2nd line containing text. Previous line did not contain any HTML
        // entities. Generate an accumulator containing previous line and a
        // trailing space. Current line will be added to the accumulator after
        // it.
        let mut buffer = String::with_capacity(trimmed_line.len() * 2); // rough estimate
        buffer.push_str(only_line_content);
        buffer.push(' ');
        *acc = Some(buffer);
    }

    // [TODO]: Decode any HTML entities in this line

    // For now, just use the trimmed line directly
    if let Some(buffer) = acc.as_mut() {
        buffer.push_str(trimmed_line);
    } else {
        // This is the first line containing text, and there are no HTML entities in
        // this line. Record this line in `only_line`.
        // If this turns out to be the only line, we won't need to construct a String,
        // so avoid all copying.
        *only_line = Some(trimmed_line);
    }
}

fn jsx_attr_value_to_expr(v: JSXAttrValue) -> Option<Box<Expr>> {
    Some(match v {
        JSXAttrValue::Str(s) => {
            let value = transform_jsx_attr_str(&s.value);

            Lit::Str(Str {
                span: s.span,
                raw: None,
                value: value.into(),
            })
            .into()
        }
        JSXAttrValue::JSXExprContainer(e) => match e.expr {
            JSXExpr::JSXEmptyExpr(_) => None?,
            JSXExpr::Expr(e) => e,
            #[cfg(swc_ast_unknown)]
            _ => panic!("unable to access unknown nodes"),
        },
        JSXAttrValue::JSXElement(e) => e.into(),
        JSXAttrValue::JSXFragment(f) => f.into(),
        #[cfg(swc_ast_unknown)]
        _ => panic!("unable to access unknown nodes"),
    })
}

fn transform_jsx_attr_str(v: &Wtf8) -> Wtf8Buf {
    // Fast path: check if transformation is needed
    let needs_transform = v.code_points().any(|cp| {
        if let Some(c) = cp.to_char() {
            matches!(
                c,
                '\u{0008}' | '\u{000c}' | '\n' | '\r' | '\t' | '\u{000b}' | '\0'
            )
        } else {
            false
        }
    });

    if !needs_transform {
        return v.to_owned();
    }

    let single_quote = false;
    let mut buf = Wtf8Buf::with_capacity(v.len());
    let mut iter = v.code_points().peekable();

    while let Some(code_point) = iter.next() {
        if let Some(c) = code_point.to_char() {
            match c {
                '\u{0008}' => buf.push_str("\\b"),
                '\u{000c}' => buf.push_str("\\f"),
                ' ' => buf.push_char(' '),

                '\n' | '\r' | '\t' => {
                    buf.push_char(' ');

                    while let Some(next) = iter.peek() {
                        if next.to_char() == Some(' ') {
                            iter.next();
                        } else {
                            break;
                        }
                    }
                }
                '\u{000b}' => buf.push_str("\\v"),
                '\0' => buf.push_str("\\x00"),

                '\'' if single_quote => buf.push_str("\\'"),
                '"' if !single_quote => buf.push_char('"'),

                '\x01'..='\x0f' | '\x10'..='\x1f' => {
                    buf.push_char(c);
                }

                '\x20'..='\x7e' => {
                    //
                    buf.push_char(c);
                }
                '\u{7f}'..='\u{ff}' => {
                    buf.push_char(c);
                }

                _ => {
                    buf.push_char(c);
                }
            }
        } else {
            buf.push(code_point);
        }
    }

    buf
}
