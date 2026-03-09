use std::cell::RefCell;

use swc_atoms::atom;
use swc_common::{Mark, SyntaxContext, DUMMY_SP};
use swc_ecma_ast::*;
use swc_ecma_utils::{prepend_stmts, quote_ident, DropSpan, ExprFactory};
use swc_ecma_visit::{noop_visit_mut_type, visit_mut_pass, VisitMut, VisitMutWith};

#[macro_export]
macro_rules! enable_helper {
    ($i:ident) => {{
        $crate::helpers::HELPERS.with(|helpers| {
            helpers.$i();
            helpers.mark()
        })
    }};
}

#[cfg(feature = "inline-helpers")]
fn parse(code: &str) -> Vec<Stmt> {
    let cm = swc_common::SourceMap::default();

    let fm = cm.new_source_file(
        swc_common::FileName::Custom(stringify!($name).into()).into(),
        code.to_string(),
    );
    swc_ecma_parser::parse_file_as_script(
        &fm,
        Default::default(),
        Default::default(),
        None,
        &mut Vec::new(),
    )
    .map(|mut script| {
        script.body.visit_mut_with(&mut DropSpan);
        script.body
    })
    .map_err(|e| {
        unreachable!("Error occurred while parsing error: {:?}", e);
    })
    .unwrap()
}

#[cfg(feature = "inline-helpers")]
macro_rules! add_to {
    ($buf:expr, $name:ident, $b:expr, $mark:expr) => {{
        static STMTS: once_cell::sync::Lazy<Vec<Stmt>> = once_cell::sync::Lazy::new(|| {
            let code = include_str!(concat!("./_", stringify!($name), ".js"));
            parse(&code)
        });

        let enable = $b;
        if enable {
            $buf.extend(STMTS.iter().cloned().map(|mut stmt| {
                stmt.visit_mut_with(&mut Marker {
                    base: SyntaxContext::empty().apply_mark($mark),
                    decls: Default::default(),

                    decl_ctxt: SyntaxContext::empty().apply_mark(Mark::new()),
                });
                stmt
            }))
        }
    }};
}

macro_rules! add_import_to {
    ($buf:expr, $name:ident, $b:expr, $mark:expr) => {{
        let enable = $b;
        if enable {
            let ctxt = SyntaxContext::empty().apply_mark($mark);
            let s = ImportSpecifier::Named(ImportNamedSpecifier {
                span: DUMMY_SP,
                local: Ident::new(concat!("_", stringify!($name)).into(), DUMMY_SP, ctxt),
                imported: Some(quote_ident!("_").into()),
                is_type_only: false,
            });

            let src: Str = concat!("@swc/helpers/_/_", stringify!($name)).into();

            $buf.push(ModuleItem::ModuleDecl(ModuleDecl::Import(ImportDecl {
                span: DUMMY_SP,
                specifiers: vec![s],
                src: Box::new(src),
                with: Default::default(),
                type_only: Default::default(),
                phase: Default::default(),
            })))
        }
    }};
}

better_scoped_tls::scoped_tls!(
    /// This variable is used to manage helper scripts like `_inherits` from babel.
    ///
    /// The instance contains flags where each flag denotes if a helper script should be injected.
    pub static HELPERS: Helpers
);

/// Tracks used helper methods. (e.g. __extends)
#[derive(Debug, Default)]
pub struct Helpers {
    external: bool,
    mark: HelperMark,
    inner: RefCell<Inner>,
}

#[derive(Debug, Clone, Copy)]
pub struct HelperData {
    external: bool,
    mark: HelperMark,
    inner: Inner,
}

impl Helpers {
    pub fn new(external: bool) -> Self {
        Helpers {
            external,
            mark: Default::default(),
            inner: Default::default(),
        }
    }

    pub const fn mark(&self) -> Mark {
        self.mark.0
    }

    pub const fn external(&self) -> bool {
        self.external
    }

    pub fn data(&self) -> HelperData {
        HelperData {
            inner: *self.inner.borrow(),
            external: self.external,
            mark: self.mark,
        }
    }

    pub fn from_data(data: HelperData) -> Self {
        Helpers {
            external: data.external,
            mark: data.mark,
            inner: RefCell::new(data.inner),
        }
    }
}

#[derive(Debug, Clone, Copy)]
struct HelperMark(Mark);
impl Default for HelperMark {
    fn default() -> Self {
        HelperMark(Mark::new())
    }
}

macro_rules! define_helpers {
    (
        Helpers {
            $( $name:ident : ( $( $dep:ident ),* ), )*
        }
    ) => {
        #[derive(Debug,Default, Clone, Copy)]
        struct Inner {
            $( $name: bool, )*
        }

        impl Helpers {
            $(
                pub fn $name(&self) {
                    self.inner.borrow_mut().$name = true;

                    if !self.external {
                        $(
                            self.$dep();
                        )*
                    }
                }
            )*
        }


        impl Helpers {
            pub fn extend_from(&self, other: &Self) {
                let other = other.inner.borrow();
                let mut me = self.inner.borrow_mut();
                $(
                    if other.$name {
                        me.$name = true;
                    }
                )*
            }
        }

        impl InjectHelpers {
            fn is_helper_used(&self) -> bool{

                HELPERS.with(|helpers|{
                    let inner = helpers.inner.borrow();
                    false $(
                      || inner.$name
                    )*
                })
            }

            #[cfg(feature = "inline-helpers")]
            fn build_helpers(&self) -> Vec<Stmt> {
                let mut buf = Vec::new();

                HELPERS.with(|helpers|{
                    let inner = helpers.inner.borrow();
                    $(
                            add_to!(buf, $name, inner.$name, helpers.mark.0);
                    )*
                });

                buf
            }

            fn build_imports(&self) -> Vec<ModuleItem> {
                let mut buf = Vec::new();

                HELPERS.with(|helpers|{
                    let inner = helpers.inner.borrow();
                    $(
                            add_import_to!(buf, $name, inner.$name, helpers.mark.0);
                    )*
                });

                buf
            }

            fn build_requires(&self) -> Vec<Stmt>{
                let mut buf = Vec::new();
                HELPERS.with(|helpers|{
                    let inner = helpers.inner.borrow();
                    $(
                        let enable = inner.$name;
                        if enable {
                            buf.push(self.build_reqire(stringify!($name), helpers.mark.0))
                        }
                        // add_require_to!(buf, $name, helpers.inner.$name, helpers.mark.0, self.global_mark);
                    )*
                });
                buf
            }
        }
    };
}

define_helpers!(Helpers {
    apply_decorated_descriptor: (),
    array_like_to_array: (),
    array_with_holes: (),
    array_without_holes: (array_like_to_array),
    assert_this_initialized: (),
    async_generator: (overload_yield),
    async_generator_delegate: (overload_yield),
    async_iterator: (),
    async_to_generator: (),
    await_async_generator: (overload_yield),
    await_value: (),
    call_super: (
        get_prototype_of,
        is_native_reflect_construct,
        possible_constructor_return
    ),
    check_private_redeclaration: (),
    class_apply_descriptor_destructure: (),
    class_apply_descriptor_get: (),
    class_apply_descriptor_set: (),
    class_apply_descriptor_update: (),
    class_call_check: (),
    class_check_private_static_field_descriptor: (),
    class_extract_field_descriptor: (),
    class_name_tdz_error: (),
    class_private_field_get: (class_extract_field_descriptor, class_apply_descriptor_get),
    class_private_field_init: (check_private_redeclaration),
    class_private_field_loose_base: (),
    class_private_field_loose_key: (),
    class_private_field_set: (class_extract_field_descriptor, class_apply_descriptor_set),
    class_private_field_update: (
        class_extract_field_descriptor,
        class_apply_descriptor_update
    ),
    class_private_method_get: (),
    class_private_method_init: (check_private_redeclaration),
    class_private_method_set: (),
    class_static_private_field_spec_get: (
        class_check_private_static_access,
        class_check_private_static_field_descriptor,
        class_apply_descriptor_get
    ),
    class_static_private_field_spec_set: (
        class_check_private_static_access,
        class_check_private_static_field_descriptor,
        class_apply_descriptor_set
    ),
    class_static_private_field_update: (
        class_check_private_static_access,
        class_check_private_static_field_descriptor,
        class_apply_descriptor_update
    ),
    construct: (is_native_reflect_construct, set_prototype_of),
    create_class: (),
    decorate: (to_array, to_property_key),
    defaults: (),
    define_enumerable_properties: (),
    define_property: (),
    export_star: (),
    extends: (),
    get: (super_prop_base),
    get_prototype_of: (),
    inherits: (set_prototype_of),
    inherits_loose: (),
    initializer_define_property: (),
    initializer_warning_helper: (),
    instanceof: (),
    interop_require_default: (),
    interop_require_wildcard: (),
    is_native_function: (),
    iterable_to_array: (),
    iterable_to_array_limit: (),
    iterable_to_array_limit_loose: (),
    jsx: (),
    new_arrow_check: (),
    non_iterable_rest: (),
    non_iterable_spread: (),
    object_destructuring_empty: (),
    object_spread: (define_property),
    object_spread_props: (),
    object_without_properties: (object_without_properties_loose),
    object_without_properties_loose: (),
    overload_yield: (),
    possible_constructor_return: (type_of, assert_this_initialized),
    read_only_error: (),
    set: (super_prop_base, define_property),
    set_prototype_of: (),
    skip_first_generator_next: (),
    sliced_to_array: (
        array_with_holes,
        iterable_to_array_limit,
        unsupported_iterable_to_array,
        non_iterable_rest
    ),
    sliced_to_array_loose: (
        array_with_holes,
        iterable_to_array_limit_loose,
        unsupported_iterable_to_array,
        non_iterable_rest
    ),
    super_prop_base: (get_prototype_of),
    tagged_template_literal: (),
    tagged_template_literal_loose: (),
    // temporal_ref: (temporal_undefined),
    // temporal_undefined: (),
    throw: (),
    to_array: (
        array_with_holes,
        iterable_to_array,
        unsupported_iterable_to_array,
        non_iterable_rest
    ),
    to_consumable_array: (
        array_without_holes,
        iterable_to_array,
        unsupported_iterable_to_array,
        non_iterable_spread
    ),
    to_primitive: (type_of),
    to_property_key: (type_of, to_primitive),
    update: (get, set),
    type_of: (),
    unsupported_iterable_to_array: (array_like_to_array),
    wrap_async_generator: (async_generator),
    wrap_native_super: (
        construct,
        get_prototype_of,
        set_prototype_of,
        is_native_function
    ),
    write_only_error: (),

    class_private_field_destructure: (
        class_extract_field_descriptor,
        class_apply_descriptor_destructure
    ),
    class_static_private_field_destructure: (
        class_check_private_static_access,
        class_extract_field_descriptor,
        class_apply_descriptor_destructure
    ),

    class_static_private_method_get: (class_check_private_static_access),
    class_check_private_static_access: (),

    is_native_reflect_construct: (),

    create_super: (
        get_prototype_of,
        is_native_reflect_construct,
        possible_constructor_return
    ),

    create_for_of_iterator_helper_loose: (unsupported_iterable_to_array),

    ts_decorate: (),
    ts_generator: (),
    ts_metadata: (),
    ts_param: (),
    ts_values: (),
    ts_add_disposable_resource: (),
    ts_dispose_resources: (),
    ts_rewrite_relative_import_extension: (),

    apply_decs_2203_r: (),
    identity: (),
    dispose: (),
    using: (),
    using_ctx: (),
});

pub fn inject_helpers(global_mark: Mark) -> impl Pass + VisitMut {
    visit_mut_pass(InjectHelpers {
        global_mark,
        helper_ctxt: None,
    })
}

struct InjectHelpers {
    global_mark: Mark,
    helper_ctxt: Option<SyntaxContext>,
}

impl InjectHelpers {
    #[allow(unused_variables)]
    fn make_helpers_for_module(&mut self) -> Vec<ModuleItem> {
        let (helper_mark, external) = HELPERS.with(|helper| (helper.mark(), helper.external()));

        #[cfg(feature = "inline-helpers")]
        if !external {
            return self
                .build_helpers()
                .into_iter()
                .map(ModuleItem::Stmt)
                .collect();
        }

        if self.is_helper_used() {
            self.helper_ctxt = Some(SyntaxContext::empty().apply_mark(helper_mark));
            self.build_imports()
        } else {
            Vec::new()
        }
    }

    #[allow(unused_variables)]
    fn make_helpers_for_script(&mut self) -> Vec<Stmt> {
        let (helper_mark, external) = HELPERS.with(|helper| (helper.mark(), helper.external()));

        #[cfg(feature = "inline-helpers")]
        if !external {
            return self.build_helpers();
        }

        if self.is_helper_used() {
            self.helper_ctxt = Some(SyntaxContext::empty().apply_mark(helper_mark));
            self.build_requires()
        } else {
            Default::default()
        }
    }

    fn build_reqire(&self, name: &str, mark: Mark) -> Stmt {
        let c = CallExpr {
            span: DUMMY_SP,
            callee: Expr::from(Ident {
                span: DUMMY_SP,
                ctxt: SyntaxContext::empty().apply_mark(self.global_mark),
                sym: atom!("require"),
                ..Default::default()
            })
            .as_callee(),
            args: vec![Str {
                span: DUMMY_SP,
                value: format!("@swc/helpers/_/_{name}").into(),
                raw: None,
            }
            .as_arg()],
            ..Default::default()
        };
        let ctxt = SyntaxContext::empty().apply_mark(mark);
        VarDecl {
            kind: VarDeclKind::Var,
            decls: vec![VarDeclarator {
                span: DUMMY_SP,
                name: Pat::Ident(Ident::new(format!("_{name}").into(), DUMMY_SP, ctxt).into()),
                init: Some(c.into()),
                definite: false,
            }],
            ..Default::default()
        }
        .into()
    }

    fn map_helper_ref_ident(&mut self, ref_ident: &Ident) -> Option<Expr> {
        self.helper_ctxt
            .filter(|ctxt| ctxt == &ref_ident.ctxt)
            .map(|_| {
                let ident = ref_ident.clone().without_loc();

                MemberExpr {
                    span: ref_ident.span,
                    obj: Box::new(ident.into()),
                    prop: MemberProp::Ident(atom!("_").into()),
                }
                .into()
            })
    }
}

impl VisitMut for InjectHelpers {
    noop_visit_mut_type!();

    fn visit_mut_module(&mut self, module: &mut Module) {
        let helpers = self.make_helpers_for_module();

        prepend_stmts(&mut module.body, helpers.into_iter());
    }

    fn visit_mut_script(&mut self, script: &mut Script) {
        let helpers = self.make_helpers_for_script();
        let helpers_is_empty = helpers.is_empty();

        prepend_stmts(&mut script.body, helpers.into_iter());

        if !helpers_is_empty {
            script.visit_mut_children_with(self);
        }
    }

    fn visit_mut_expr(&mut self, n: &mut Expr) {
        match n {
            Expr::Ident(ref_ident) => {
                if let Some(expr) = self.map_helper_ref_ident(ref_ident) {
                    *n = expr;
                }
            }

            _ => n.visit_mut_children_with(self),
        };
    }
}

#[cfg(feature = "inline-helpers")]
struct Marker {
    base: SyntaxContext,
    decls: rustc_hash::FxHashMap<swc_atoms::Atom, SyntaxContext>,

    decl_ctxt: SyntaxContext,
}

#[cfg(feature = "inline-helpers")]
impl VisitMut for Marker {
    noop_visit_mut_type!();

    fn visit_mut_fn_decl(&mut self, n: &mut FnDecl) {
        let old_decl_ctxt = std::mem::replace(
            &mut self.decl_ctxt,
            SyntaxContext::empty().apply_mark(Mark::new()),
        );
        let old_decls = self.decls.clone();

        n.visit_mut_children_with(self);

        self.decls = old_decls;
        self.decl_ctxt = old_decl_ctxt;
    }

    fn visit_mut_fn_expr(&mut self, n: &mut FnExpr) {
        let old_decl_ctxt = std::mem::replace(
            &mut self.decl_ctxt,
            SyntaxContext::empty().apply_mark(Mark::new()),
        );
        let old_decls = self.decls.clone();

        n.visit_mut_children_with(self);

        self.decls = old_decls;
        self.decl_ctxt = old_decl_ctxt;
    }

    fn visit_mut_ident(&mut self, i: &mut Ident) {
        i.ctxt = self.decls.get(&i.sym).copied().unwrap_or(self.base);
    }

    fn visit_mut_member_prop(&mut self, p: &mut MemberProp) {
        if let MemberProp::Computed(p) = p {
            p.visit_mut_with(self);
        }
    }

    fn visit_mut_param(&mut self, n: &mut Param) {
        if let Pat::Ident(i) = &n.pat {
            self.decls.insert(i.sym.clone(), self.decl_ctxt);
        }

        n.visit_mut_children_with(self);
    }

    fn visit_mut_prop_name(&mut self, n: &mut PropName) {
        if let PropName::Computed(e) = n {
            e.visit_mut_with(self);
        }
    }

    fn visit_mut_super_prop(&mut self, p: &mut SuperProp) {
        if let SuperProp::Computed(p) = p {
            p.visit_mut_with(self);
        }
    }

    fn visit_mut_var_declarator(&mut self, v: &mut VarDeclarator) {
        if let Pat::Ident(i) = &mut v.name {
            if &*i.sym == "id" || &*i.sym == "resource" {
                i.ctxt = self.base;
                self.decls.insert(i.sym.clone(), self.base);
                return;
            }

            if !(i.sym.starts_with("__") && i.sym.starts_with("_ts_")) {
                self.decls.insert(i.sym.clone(), self.decl_ctxt);
            }
        }

        v.visit_mut_children_with(self);
    }
}

#[cfg(test)]
mod tests {
    use testing::DebugUsingDisplay;

    use super::*;

    #[test]
    fn external_helper() {
        let input = "_throw()";
        crate::tests::Tester::run(|tester| {
            HELPERS.set(&Helpers::new(true), || {
                let expected = tester.apply_transform(
                    DropSpan,
                    "output.js",
                    Default::default(),
                    "import { _ as _throw } from \"@swc/helpers/_/_throw\";
_throw();",
                )?;
                enable_helper!(throw);

                eprintln!("----- Actual -----");

                let tr = inject_helpers(Mark::new());
                let actual = tester
                    .apply_transform(tr, "input.js", Default::default(), input)?
                    .apply(crate::hygiene::hygiene())
                    .apply(crate::fixer::fixer(None));

                if actual == expected {
                    return Ok(());
                }

                let (actual_src, expected_src) = (tester.print(&actual), tester.print(&expected));

                if actual_src == expected_src {
                    return Ok(());
                }

                println!(">>>>> Orig <<<<<\n{input}");
                println!(">>>>> Code <<<<<\n{actual_src}");
                assert_eq!(
                    DebugUsingDisplay(&actual_src),
                    DebugUsingDisplay(&expected_src)
                );
                Err(())
            })
        });
    }

    #[test]
    #[cfg(feature = "inline-helpers")]
    fn use_strict_before_helper() {
        crate::tests::test_transform(
            Default::default(),
            |_| {
                enable_helper!(throw);
                inject_helpers(Mark::new())
            },
            "'use strict'",
            "'use strict'
function _throw(e) {
    throw e;
}
",
            false,
            Default::default,
        )
    }

    #[test]
    #[cfg(feature = "inline-helpers")]
    fn name_conflict() {
        crate::tests::test_transform(
            Default::default(),
            |_| {
                enable_helper!(throw);
                inject_helpers(Mark::new())
            },
            "let _throw = null",
            "function _throw(e) {
    throw e;
}
let _throw1 = null;
",
            false,
            Default::default,
        )
    }

    #[test]
    fn use_strict_abort() {
        crate::tests::test_transform(
            Default::default(),
            |_| noop_pass(),
            "'use strict'

let x = 4;",
            "'use strict'

let x = 4;",
            false,
            Default::default,
        );
    }

    #[test]
    #[cfg(feature = "inline-helpers")]
    fn issue_8871() {
        crate::tests::test_transform(
            Default::default(),
            |_| {
                enable_helper!(using_ctx);
                inject_helpers(Mark::new())
            },
            "let _throw = null",
            r#"
            function _using_ctx() {
                var _disposeSuppressedError = typeof SuppressedError === "function" ? SuppressedError : function(error, suppressed) {
                    var err = new Error();
                    err.name = "SuppressedError";
                    err.suppressed = suppressed;
                    err.error = error;
                    return err;
                }, empty = {}, stack = [];
                function using(isAwait, value) {
                    if (value != null) {
                        if (Object(value) !== value) {
                            throw new TypeError("using declarations can only be used with objects, functions, null, or undefined.");
                        }
                        if (isAwait) {
                            var dispose = value[Symbol.asyncDispose || Symbol.for("Symbol.asyncDispose")];
                        }
                        if (dispose == null) {
                            dispose = value[Symbol.dispose || Symbol.for("Symbol.dispose")];
                        }
                        if (typeof dispose !== "function") {
                            throw new TypeError(`Property [Symbol.dispose] is not a function.`);
                        }
                        stack.push({
                            v: value,
                            d: dispose,
                            a: isAwait
                        });
                    } else if (isAwait) {
                        stack.push({
                            d: value,
                            a: isAwait
                        });
                    }
                    return value;
                }
                return {
                    e: empty,
                    u: using.bind(null, false),
                    a: using.bind(null, true),
                    d: function() {
                        var error = this.e;
                        function next() {
                            while(resource = stack.pop()){
                                try {
                                    var resource, disposalResult = resource.d && resource.d.call(resource.v);
                                    if (resource.a) {
                                        return Promise.resolve(disposalResult).then(next, err);
                                    }
                                } catch (e) {
                                    return err(e);
                                }
                            }
                            if (error !== empty) throw error;
                        }
                        function err(e) {
                            error = error !== empty ? new _disposeSuppressedError(error, e) : e;
                            return next();
                        }
                        return next();
                    }
                };
            }
                    
let _throw = null;
"#,
            false,
            Default::default,
        )
    }
}
