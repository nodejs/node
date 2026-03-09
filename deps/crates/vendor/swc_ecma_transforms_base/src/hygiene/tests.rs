use rustc_hash::FxHashMap;
use swc_atoms::{atom, Atom};
use swc_common::{hygiene::*, DUMMY_SP};
use swc_ecma_parser::Syntax;
use swc_ecma_utils::quote_ident;
use swc_ecma_visit::{Fold, FoldWith};
use testing::{assert_eq, DebugUsingDisplay};

use super::*;
use crate::{
    rename::renamer_keep_contexts,
    tests::{HygieneVisualizer, Tester},
};

struct Marker {
    map: FxHashMap<Atom, Mark>,
}

fn marker(markers: &[(&str, Mark)]) -> Marker {
    Marker {
        map: markers.iter().map(|(k, v)| ((*k).into(), *v)).collect(),
    }
}

impl Fold for Marker {
    fn fold_ident(&mut self, mut ident: Ident) -> Ident {
        if let Some(mark) = self.map.get(&ident.sym) {
            ident.ctxt = ident.ctxt.apply_mark(*mark);
        }

        ident
    }
}

struct OnceMarker {
    map: FxHashMap<Atom, Vec<Mark>>,
}

impl OnceMarker {
    fn new(markers: &[(&str, &[Mark])]) -> OnceMarker {
        OnceMarker {
            map: markers
                .iter()
                .map(|(k, v)| ((*k).into(), (*v).into()))
                .collect(),
        }
    }
}

impl Fold for OnceMarker {
    fn fold_ident(&mut self, mut ident: Ident) -> Ident {
        if let Some(marks) = self.map.get_mut(&ident.sym) {
            ident.ctxt = ident.ctxt.apply_mark(marks.remove(0));
        }

        ident
    }

    fn fold_prop_name(&mut self, prop: PropName) -> PropName {
        match prop {
            PropName::Computed(_) => prop.fold_children_with(self),
            _ => prop,
        }
    }
}

/// Apply the n-th mark to idents of the form `name$2`.
struct InlineContextMarker {
    marks: Vec<Mark>,
}
impl VisitMut for InlineContextMarker {
    fn visit_mut_ident(&mut self, ident: &mut Ident) {
        let sym = ident.sym.to_string();
        let split = sym.split("$").collect::<Vec<_>>();
        if let [name, index] = *split {
            ident.sym = name.into();
            ident.ctxt = ident
                .ctxt
                .apply_mark(self.marks[index.parse::<usize>().unwrap()]);
        } else {
            panic!("couldn't find index in ident");
        }
    }
}

fn test<F>(op: F, expected: &str)
where
    F: FnOnce(&mut Tester<'_>) -> Result<Vec<Stmt>, ()>,
{
    test_module(
        |tester| {
            Ok(Module {
                span: DUMMY_SP,
                body: op(tester)?.into_iter().map(ModuleItem::Stmt).collect(),
                shebang: None,
            })
        },
        expected,
        Default::default,
    )
}

fn test_module<F>(op: F, expected: &str, config: impl FnOnce() -> Config)
where
    F: FnOnce(&mut crate::tests::Tester<'_>) -> Result<Module, ()>,
{
    crate::tests::Tester::run(|tester| {
        let mut module = Program::Module(op(tester)?);

        let hygiene_src = tester.print(&module.clone().fold_with(&mut HygieneVisualizer));
        println!("----- Hygiene -----\n{hygiene_src}");

        hygiene_with_config(config()).process(&mut module);

        let actual = tester.print(&module);

        let expected = {
            let expected = tester
                .with_parser("expected.js", Syntax::default(), expected, |p| {
                    p.parse_module()
                })
                .map(Program::Module)?;
            tester.print(&expected)
        };

        if actual != expected {
            println!("----- Actual -----\n{actual}");
            println!("----- Diff -----");

            assert_eq!(DebugUsingDisplay(&actual), DebugUsingDisplay(&expected));
        }

        Ok(())
    })
}

#[test]
fn simple() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(vec![
                tester
                    .parse_stmt("actual1.js", "var foo = 1;")?
                    .fold_with(&mut marker(&[("foo", mark1)])),
                tester
                    .parse_stmt("actual2.js", "var foo = 2;")?
                    .fold_with(&mut marker(&[("foo", mark2)])),
                tester
                    .parse_stmt("actual3.js", "use(foo)")?
                    .fold_with(&mut marker(&[("foo", mark1)])),
            ])
        },
        "
        var foo = 1;
        var foo1 = 2;
        use(foo);
        ",
    );
}

#[test]
fn block_scoping_with_usage() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            let stmts = vec![
                tester
                    .parse_stmt("actual1.js", "var foo = 1;")?
                    .fold_with(&mut marker(&[("foo", mark1)])),
                tester
                    .parse_stmt("actual2.js", "{ let foo = 2; use(foo); }")?
                    .fold_with(&mut marker(&[("foo", mark2)])),
                tester
                    .parse_stmt("actual3.js", "use(foo)")?
                    .fold_with(&mut marker(&[("foo", mark1)])),
            ];
            Ok(stmts)
        },
        "
        var foo = 1;
        {
            let foo = 2;
            use(foo);
        }
        use(foo);",
    );
}

#[test]
fn block_scoping_no_usage() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(mark1);

            let stmts = vec![
                tester
                    .parse_stmt("actual1.js", "let foo;")?
                    .fold_with(&mut marker(&[("foo", mark1)])),
                tester
                    .parse_stmt("actual2.js", "{ let foo }")?
                    .fold_with(&mut marker(&[("foo", mark2)])),
            ];
            Ok(stmts)
        },
        "
        let foo;
        {
            let foo;
        }
        ",
    );
}

#[test]
fn fn_binding_ident() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(vec![
                tester
                    .parse_stmt("actual1.js", "var foo = function baz(){}")?
                    .fold_with(&mut marker(&[("baz", mark1)])),
                tester
                    .parse_stmt("actual2.js", "var bar = function baz(){};")?
                    .fold_with(&mut marker(&[("baz", mark2)])),
                tester
                    .parse_stmt("actual3.js", "use(baz)")?
                    .fold_with(&mut marker(&[("baz", mark1)])),
            ])
        },
        "var foo = function baz(){};
            var bar = function baz(){};
            use(baz);",
    );
}

#[test]
fn fn_binding_ident_in_call() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(mark1);
            let mark3 = Mark::fresh(mark1);

            Ok(vec![
                tester
                    .parse_stmt("actual1.js", "var foo = use(function baz(){})")?
                    .fold_with(&mut marker(&[("baz", mark1)])),
                tester
                    .parse_stmt("actual2.js", "var bar1 = use(function baz(){})")?
                    .fold_with(&mut marker(&[("baz", mark2)])),
                tester
                    .parse_stmt("actual3.js", "var bar2 = use(function baz(){})")?
                    .fold_with(&mut marker(&[("baz", mark3)])),
                tester
                    .parse_stmt("actual4.js", "use(baz)")?
                    .fold_with(&mut marker(&[("baz", mark1)])),
            ])
        },
        "var foo = use(function baz(){});
            var bar1 = use(function baz(){});
            var bar2 = use(function baz(){});
            use(baz);",
    );
}

#[test]
fn member_expr() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(mark1);

            Ok(vec![
                tester
                    .parse_stmt("actual1.js", "let a;")?
                    .fold_with(&mut marker(&[("a", mark1)])),
                tester
                    .parse_stmt("actual2.js", "foo.a = init")?
                    .fold_with(&mut marker(&[("a", mark2)])),
            ])
        },
        "let a;
            foo.a = init",
    );
}

#[test]
fn const_then_fn_param() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(mark1);

            Ok(vec![
                tester
                    .parse_stmt("actual1.js", "const a = 1;")?
                    .fold_with(&mut marker(&[("a", mark1)])),
                tester
                    .parse_stmt("actual2.js", "function foo(a) {use(a);}")?
                    .fold_with(&mut marker(&[("a", mark2)])),
            ])
        },
        "const a = 1;
            function foo(a) {
                use(a);
            }",
    );
}

#[test]
fn for_loop() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(mark1);
            let mark3 = Mark::fresh(mark1);

            Ok(vec![
                tester
                    .parse_stmt("actual1.js", "for(var a=1;;) {}")?
                    .fold_with(&mut marker(&[("a", mark1)])),
                tester
                    .parse_stmt("actual2.js", "for(var a of foo) {}")?
                    .fold_with(&mut marker(&[("a", mark2)])),
                tester
                    .parse_stmt("actual3.js", "for(var a=3;;) {}")?
                    .fold_with(&mut marker(&[("a", mark3)])),
            ])
        },
        "
            for(var a=1;;) {}
            for(var a1 of foo) {}
            for(var a2 = 3;;) {}
            ",
    );
}

#[test]
fn try_for_loop() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(mark1);
            let mark3 = Mark::fresh(mark1);

            Ok(vec![
                tester
                    .parse_stmt("actual1.js", "try { for(var a=1;;) {} } finally {}")?
                    .fold_with(&mut marker(&[("a", mark1)])),
                tester
                    .parse_stmt("actual2.js", "for(var a of foo) {}")?
                    .fold_with(&mut marker(&[("a", mark2)])),
                tester
                    .parse_stmt("actual3.js", "for(var a=3;;) {}")?
                    .fold_with(&mut marker(&[("a", mark3)])),
            ])
        },
        "
            try {
                for(var a=1;;) {}
            } finally {
            }
            for(var a1 of foo) {}
            for(var a2 = 3;;) {}
            ",
    );
}

#[test]
fn shorthand() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(mark1);

            Ok(vec![
                tester
                    .parse_stmt("actual1.js", "let a = 1;")?
                    .fold_with(&mut marker(&[("a", mark1)])),
                tester
                    .parse_stmt(
                        "actual2.js",
                        "function foo() {
                                let a = 2;
                                use({ a })
                            }",
                    )?
                    .fold_with(&mut marker(&[("a", mark2)])),
            ])
        },
        "
            let a = 1;
            function foo() {
                let a = 2;
                use({ a })
            }
            ",
    );
}

#[test]
fn same_mark() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());

            Ok(vec![
                tester
                    .parse_stmt("actual1.js", "var a = 1;")?
                    .fold_with(&mut marker(&[("a", mark1)])),
                tester
                    .parse_stmt("actual2.js", "var a = 1;")?
                    .fold_with(&mut marker(&[("a", mark1)])),
            ])
        },
        "
            var a = 1;
            var a = 1;
            ",
    );
}

#[test]
fn mark_root() {
    test(
        |tester| {
            let mark2 = Mark::fresh(Mark::root());

            Ok(vec![
                tester.parse_stmt("actual1.js", "var foo = 'bar';")?,
                Stmt::Decl(Decl::Fn(FnDecl {
                    ident: quote_ident!("Foo").into(),
                    function: Box::new(Function {
                        span: DUMMY_SP,
                        is_async: false,
                        is_generator: false,
                        decorators: Vec::new(),
                        body: Some(BlockStmt {
                            stmts: vec![
                                tester
                                    .parse_stmt("actual2.js", "var foo = 'foo';")?
                                    .fold_with(&mut marker(&[("foo", mark2)])),
                                tester.parse_stmt(
                                    "actual3.js",
                                    "_define_property(this, 'bar', foo);",
                                )?,
                            ],
                            ..Default::default()
                        }),
                        params: Vec::new(),
                        ..Default::default()
                    }),

                    declare: false,
                })),
            ])
        },
        "
var foo = 'bar';
function Foo() {
    var foo1 = 'foo';
    _define_property(this, 'bar', foo);
}
            ",
    );
}

#[test]
fn var_class_decl() {
    test(
        |tester| {
            Ok(vec![
                tester.parse_stmt("actual1.js", "var Foo = function Foo(){}")?
            ])
        },
        "var Foo = function Foo(){}",
    );
}

#[test]
fn var_class_decl_2() {
    test(
        |tester| {
            Ok(vec![tester
                .parse_stmt(
                    "actual1.js",
                    "
                var Foo = (function() {
                    function Foo() {}
                    return Foo;
                }())
                ",
                )?
                .fold_with(&mut marker(&[(
                    "Foo",
                    Mark::fresh(Mark::root()),
                )]))])
        },
        "
        var Foo = (function(){
            function Foo(){

            }
            return Foo;
        }())
        ",
    );
}

#[test]
fn fn_args() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(vec![Stmt::Decl(Decl::Fn(FnDecl {
                ident: quote_ident!("Foo").into(),
                function: Box::new(Function {
                    span: DUMMY_SP,
                    is_async: false,
                    is_generator: false,
                    decorators: Vec::new(),
                    body: Some(BlockStmt {
                        stmts: vec![tester
                            .parse_stmt("actual1.js", "_define_property(this, 'force', force);")?
                            .fold_with(&mut marker(&[("force", mark2)]))],
                        ..Default::default()
                    }),
                    params: vec![Param {
                        span: DUMMY_SP,
                        decorators: Vec::new(),
                        pat: quote_ident!("force").into(),
                    }
                    .fold_with(&mut marker(&[("force", mark1)]))],
                    ..Default::default()
                }),

                declare: false,
            }))])
        },
        "
        function Foo(force1) {
            _define_property(this, 'force', force);
        }
        ",
    );
}

#[test]
fn block_in_fn() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(mark1);

            Ok(vec![Stmt::Decl(Decl::Fn(FnDecl {
                ident: quote_ident!("Foo").into(),
                function: Box::new(Function {
                    span: DUMMY_SP,
                    is_async: false,
                    is_generator: false,
                    decorators: Vec::new(),
                    body: Some(BlockStmt {
                        stmts: vec![
                            tester
                                .parse_stmt("actual1.js", "var bar;")?
                                .fold_with(&mut marker(&[("bar", mark1)])),
                            tester
                                .parse_stmt("actual2.js", "{ var bar; }")?
                                .fold_with(&mut marker(&[("bar", mark2)])),
                        ],
                        ..Default::default()
                    }),
                    params: Vec::new(),
                    ..Default::default()
                }),

                declare: false,
            }))])
        },
        "
        // Ideal:
        //
        // function Foo() {
        //     var bar;
        //     {
        //         var bar;
        //     }
        // }

        function Foo() {
            var bar;
            {
                var bar1;
            }
        }
        ",
    );
}

#[test]
fn flat_in_fn() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(mark1);

            Ok(vec![Stmt::Decl(Decl::Fn(FnDecl {
                ident: quote_ident!("Foo").into(),
                function: Box::new(Function {
                    span: DUMMY_SP,
                    is_async: false,
                    is_generator: false,
                    decorators: Vec::new(),
                    body: Some(BlockStmt {
                        stmts: vec![
                            tester
                                .parse_stmt("actual1.js", "var bar;")?
                                .fold_with(&mut marker(&[("bar", mark1)])),
                            tester
                                .parse_stmt("actual2.js", "var bar;")?
                                .fold_with(&mut marker(&[("bar", mark2)])),
                        ],
                        ..Default::default()
                    }),
                    params: Vec::new(),
                    ..Default::default()
                }),

                declare: false,
            }))])
        },
        "
        function Foo() {
            var bar;
            var bar1;
        }
        ",
    );
}

#[test]
fn params_in_fn() {
    test(
        |_tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(vec![Stmt::Decl(Decl::Fn(FnDecl {
                ident: quote_ident!("Foo").into(),
                function: Box::new(Function {
                    span: DUMMY_SP,
                    is_async: false,
                    is_generator: false,
                    decorators: Vec::new(),
                    body: Some(BlockStmt {
                        ..Default::default()
                    }),
                    params: vec![
                        Param {
                            span: DUMMY_SP,
                            decorators: Default::default(),
                            pat: Ident::new(
                                atom!("param"),
                                DUMMY_SP,
                                SyntaxContext::empty().apply_mark(mark1),
                            )
                            .into(),
                        },
                        Param {
                            span: DUMMY_SP,
                            decorators: Default::default(),
                            pat: Ident::new(
                                atom!("param"),
                                DUMMY_SP,
                                SyntaxContext::empty().apply_mark(mark2),
                            )
                            .into(),
                        },
                    ],
                    ..Default::default()
                }),

                declare: false,
            }))])
        },
        "function Foo(param, param1) {}",
    );
}

#[test]
fn next_fn() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(vec![
                tester
                    .parse_stmt("actual1.js", "function foo(param){}")?
                    .fold_with(&mut marker(&[("param", mark1)])),
                tester
                    .parse_stmt("actual2.js", "function bar(param){}")?
                    .fold_with(&mut marker(&[("param", mark2)])),
            ])
        },
        "
        function foo(param) {}
        function bar(param) {}
        ",
    );
}

#[test]
fn for_x() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(mark1);
            let mark3 = Mark::fresh(mark1);
            let mark4 = Mark::fresh(mark3);

            Ok(vec![
                tester
                    .parse_stmt(
                        "actual1.js",
                        "for (var _ref of []){
                            var { a } = _ref, b = _object_without_properties(_ref, ['a']);
                        }",
                    )?
                    .fold_with(&mut marker(&[("_ref", mark1)])),
                tester
                    .parse_stmt(
                        "actual2.js",
                        "for (var _ref of []){
                            var { a } = _ref, b = _object_without_properties(_ref, ['a']);
                        }",
                    )?
                    .fold_with(&mut marker(&[("_ref", mark2)])),
                tester
                    .parse_stmt(
                        "actual3.js",
                        "async function a() {
                            for await (var _ref of []){
                                var { a } = _ref, b = _object_without_properties(_ref, ['a']);
                            }
                        }",
                    )?
                    .fold_with(&mut marker(&[("_ref", mark4)])),
            ])
        },
        "
        for (var _ref of []){
            var { a } = _ref, b = _object_without_properties(_ref, ['a']);
        }

        for (var _ref1 of []){
            var { a } = _ref1, b = _object_without_properties(_ref1, ['a']);
        }
        async function a() {
            for await (var _ref of []){
                var { a } = _ref, b = _object_without_properties(_ref, ['a']);
            }
        }
        ",
    );
}

#[test]
fn fn_param_same_name() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(vec![tester
                .parse_stmt("actual1.js", "function foo(param, param){}")?
                .fold_with(&mut OnceMarker::new(&[(
                    "param",
                    &[mark1, mark2],
                )]))])
        },
        "function foo(param, param1){}",
    );
}

#[test]
fn fn_param_same_name_in_arg() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(vec![tester
                .parse_stmt("actual1.js", "use(function (param, param){})")?
                .fold_with(&mut OnceMarker::new(&[(
                    "param",
                    &[mark1, mark2],
                )]))])
        },
        "use(function (param, param1){})",
    );
}

#[test]
fn nested_fn_param_with_same_name() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(vec![tester
                .parse_stmt(
                    "actual1.js",
                    "
                    function _three() {
                        _three = _async_to_generator(function*(a, param, c, param) {
                        });
                        return _three.apply(this, arguments);
                    }
                    ",
                )?
                .fold_with(&mut OnceMarker::new(&[(
                    "param",
                    &[mark1, mark2],
                )]))])
        },
        "
        function _three() {
            _three = _async_to_generator(function*(a, param, c, param1) {
            });
            return _three.apply(this, arguments);
        }
        ",
    );
}

#[test]
fn regression_001() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());
            let mark3 = Mark::fresh(Mark::root());
            let mark4 = Mark::fresh(Mark::root());

            Ok(vec![tester
                .parse_stmt(
                    "actual1.js",
                    "var Foo = function() {
    function Foo() {
        _class_call_check(this, Foo);
        foo.set(this, {
             writable: true, value: 0
        });
    }
    _create_class(Foo, [{
             key: 'test', value: function test(other) {
                    var old, _obj, old, _obj;
                     _class_private_field_set(this, foo, (old = +_class_private_field_get(this, \
                     foo)) + 1), old;
                     _class_private_field_set(_obj = other.obj, foo, (old = \
                     +_class_private_field_get(_obj, foo)) + 1), old;
                }
        }]);
    return Foo;
}();
                    ",
                )?
                .fold_with(&mut OnceMarker::new(&[
                    ("old", &[mark1, mark2, mark1, mark1, mark2, mark2]),
                    ("_obj", &[mark3, mark4, mark3, mark4]),
                ]))])
        },
        "var Foo = function() {
    function Foo() {
        _class_call_check(this, Foo);
        foo.set(this, {
             writable: true, value: 0
        });
    }
    _create_class(Foo, [{
             key: 'test', value: function test(other) {
                var old, _obj, old1, _obj1;
                _class_private_field_set(this, foo, (old = +_class_private_field_get(this, foo)) + \
         1), old;
                _class_private_field_set(_obj = other.obj, foo, (old1 = \
         +_class_private_field_get(_obj1, foo)) + 1), old1;
                }
        }]);
    return Foo;
}();
        ",
    );
}

#[test]
fn regression_002() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(vec![tester
                .parse_stmt(
                    "actual1.js",
                    "_create_class(Foo, [{
             key: 'test', value: function test(other) {
                    var old, _obj, old, _obj;
                     _class_private_field_set(this, foo, (old = +_class_private_field_get(this, \
                     foo)) + 1), old;
                     _class_private_field_set(_obj = other.obj, foo, (old = \
                     +_class_private_field_get(_obj, foo)) + 1), old;
                }
        }])",
                )?
                .fold_with(&mut OnceMarker::new(&[(
                    "old",
                    &[mark1, mark2, mark1, mark1, mark2, mark2],
                )]))])
        },
        "_create_class(Foo, [{
             key: 'test', value: function test(other) {
                    var old, _obj, old1, _obj;
                     _class_private_field_set(this, foo, (old = +_class_private_field_get(this, \
         foo)) + 1), old;
                     _class_private_field_set(_obj = other.obj, foo, (old1 = \
         +_class_private_field_get(_obj, foo)) + 1), old1;
                }
        }]);",
    );
}

#[test]
fn regression_003() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(tester
                .parse_stmts(
                    "actual1.js",
                    "var old, _obj, old, _obj;
                     _class_private_field_set(this, foo, (old = +_class_private_field_get(this, \
                     foo)) + 1), old;
                     _class_private_field_set(_obj = other.obj, foo, (old = \
                     +_class_private_field_get(_obj, foo)) + 1), old;",
                )?
                .fold_with(&mut OnceMarker::new(&[(
                    "old",
                    &[mark1, mark2, mark1, mark1, mark2, mark2],
                )])))
        },
        "var old, _obj, old1, _obj;
                     _class_private_field_set(this, foo, (old = +_class_private_field_get(this, \
         foo)) + 1), old;
                     _class_private_field_set(_obj = other.obj, foo, (old1 = \
         +_class_private_field_get(_obj, foo)) + 1), old1;",
    );
}

#[test]
fn regression_004() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(tester
                .parse_stmts(
                    "actual1.js",
                    "function foo(...args){}
                    function bar(...args){}",
                )?
                .fold_with(&mut OnceMarker::new(&[("args", &[mark1, mark2])])))
        },
        "function foo(...args){}
        function bar(...args){}",
    );
}

#[test]
fn regression_005() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(tester
                .parse_stmts(
                    "actual1.js",
                    "var foo = (...args)=>{}
                    var bar = (...args)=>{}",
                )?
                .fold_with(&mut OnceMarker::new(&[("args", &[mark1, mark2])])))
        },
        "var foo = (...args)=>{}
        var bar = (...args)=>{}",
    );
}

#[test]
fn module_01() {
    test_module(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(tester
                .parse_module(
                    "actual1.js",
                    "import foo from 'src1';
                    import foo from 'src2';",
                )?
                .fold_with(&mut OnceMarker::new(&[("foo", &[mark1, mark2])])))
        },
        "import foo from 'src1';
        import foo1 from 'src2';",
        Default::default,
    );
}

#[test]
fn module_02() {
    test_module(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(tester
                .parse_module(
                    "actual1.js",
                    "import {foo} from 'src1';
                    import {foo} from 'src2';",
                )?
                .fold_with(&mut OnceMarker::new(&[("foo", &[mark1, mark2])])))
        },
        "import {foo} from 'src1';
        import {foo as foo1} from 'src2';",
        Default::default,
    );
}

#[test]
fn module_03() {
    test_module(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(tester
                .parse_module(
                    "actual1.js",
                    "var foo = 1;
                    var foo = 2;
                    export {foo}",
                )?
                .fold_with(&mut OnceMarker::new(&[("foo", &[mark1, mark2, mark2])])))
        },
        "var foo = 1;
        var foo1 = 2;
        export {foo1 as foo}",
        Default::default,
    );
}

#[test]
fn issue_281_01() {
    test_module(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());

            Ok(tester
                .parse_module(
                    "actual1.js",
                    "label: {
                        break label
                    }",
                )?
                .fold_with(&mut OnceMarker::new(&[("label", &[mark1, mark1])])))
        },
        "label: {
            break label
        }",
        Default::default,
    );
}

#[test]
fn issue_281_02() {
    test_module(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());
            let mark3 = Mark::fresh(Mark::root());

            Ok(tester
                .parse_module(
                    "actual1.js",
                    "function foo(e) {
                        e: {
                            try {
                            } catch (e) {
                                o = null;
                                break e
                            }
                        }
                    }",
                )?
                .fold_with(&mut OnceMarker::new(&[(
                    "e",
                    &[mark1, mark2, mark3, mark2],
                )])))
        },
        "function foo(e1) {
            e: {
                try {
                } catch (e1) {
                    o = null;
                    break e
                }
            }
        }",
        Default::default,
    );
}

#[test]
fn issue_295_01() {
    test_module(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());

            Ok(tester
                .parse_module(
                    "actual1.js",
                    "export const bar = {};
                    class Foo {

                      constructor() {
                            bar;
                        }
                    }",
                )?
                .fold_with(&mut OnceMarker::new(&[("bar", &[mark1, mark1, mark1])])))
        },
        "
        export const bar = {
        };
        class Foo {
            constructor(){
                bar;
            }
        }
        ",
        Default::default,
    );
}

#[test]
fn issue_295_02() {
    test_module(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(tester
                .parse_module(
                    "actual1.js",
                    "export const bar = {};
                    class Foo {

                      constructor() {
                            bar;
                        }
                    }",
                )?
                .fold_with(&mut OnceMarker::new(&[("bar", &[mark1, mark2])])))
        },
        "
        const bar1 = {
        };
        export { bar1 as bar };
        class Foo {
            constructor(){
                bar;
            }
        }
        ",
        Default::default,
    );
}

#[test]
fn exported_function() {
    test_module(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(tester
                .parse_module(
                    "actual1.js",
                    "const foo = {};
                    export function foo(){}",
                )?
                .fold_with(&mut OnceMarker::new(&[("foo", &[mark1, mark2])])))
        },
        "const foo = {};
        function foo1(){}
      export { foo1 as foo };",
        Default::default,
    );
}

#[test]
fn exported_class_1() {
    test_module(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(tester
                .parse_module(
                    "actual1.js",
                    "var Foo = {};
                    export class Foo {}",
                )?
                .fold_with(&mut OnceMarker::new(&[("Foo", &[mark1, mark2])])))
        },
        "var Foo = {};
        class Foo1 {}
        export { Foo1 as Foo };",
        Default::default,
    );
}

#[test]
fn issue_1279() {
    test_module(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(tester
                .parse_module(
                    "actual1.js",
                    "class Foo {
                        method() {
                            class Foo {}
                            new Foo();
                        }
                    }",
                )?
                .fold_with(&mut OnceMarker::new(&[("Foo", &[mark1, mark2, mark1])])))
        },
        "
        let Foo = class Foo {
            method() {
                let Foo1 = class Foo {
                };
                new Foo();
            }
        };
        ",
        || Config {
            keep_class_names: true,
            ..Default::default()
        },
    );
}

#[test]
#[ignore = "Postponed"]
fn issue_1507() {
    test_module(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            Ok(tester
                .parse_module(
                    "actual1.js",
                    "class Foo {
                        method() {
                            const cls = class Foo {
                                method() {
                                    new Foo();
                                }
                            }
                        }
                    }",
                )?
                .fold_with(&mut OnceMarker::new(&[("Foo", &[mark1, mark2, mark1])])))
        },
        "
        let Foo1 = class Foo {
            method() {
                const cls = class Foo {
                    method() {
                        new Foo1();
                    }
                };
            }
        };
        ",
        || Config {
            keep_class_names: true,
            ..Default::default()
        },
    );
}

#[test]
fn opt_1() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            let stmts = tester
                .parse_stmts(
                    "actual1.js",
                    "
                    var foo = 1;
                    {
                        const foo = 2;
                        {
                            foo = foo + foo
                        }
                    }
                    ",
                )?
                .fold_with(&mut OnceMarker::new(&[(
                    "foo",
                    &[mark1, mark2, mark1, mark2, mark1],
                )]));
            Ok(stmts)
        },
        "
        var foo = 1;
        {
            const foo1 = 2;
            {
                foo = foo1 + foo
            }
        }
        ",
    );
}

#[test]
fn opt_2() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            let mark3 = Mark::fresh(Mark::root());
            let mark4 = Mark::fresh(Mark::root());

            let stmts = tester
                .parse_stmts(
                    "actual1.js",
                    "
                    var b = 1;
                    var b1 = 2;
                    {
                        const b = 3;
                        const b1 = 4;
                        {
                            b1 = b + b + b1 + b1
                        }
                    }
                    ",
                )?
                .fold_with(&mut OnceMarker::new(&[
                    ("b", &[mark1, mark2, mark2, mark1]),
                    ("b1", &[mark3, mark4, mark3, mark4, mark3]),
                ]));
            Ok(stmts)
        },
        "
        var b = 1;
        var b1 = 2;
        {
            const b2 = 3;
            const b11 = 4;
            {
                b1 = b2 + b + b11 + b1;
            }
        }
        ",
    );
}

#[test]
fn opt_3() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            let stmts = tester
                .parse_stmts(
                    "actual1.js",
                    "
                    var e = 1;
                    try {
                        throw 2;
                    } catch (e) {
                        console.log(e);
                    }
                    ",
                )?
                .fold_with(&mut OnceMarker::new(&[("e", &[mark1, mark2, mark1])]));
            Ok(stmts)
        },
        "
        var e = 1;
        try {
            throw 2;
        } catch (e1) {
            console.log(e);
        }
        ",
    );
}

#[test]
fn opt_4() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            let stmts = tester
                .parse_stmts(
                    "actual1.js",
                    "
                    const obj = {
                        key: function a() {
                            a()
                        }
                    }
                    function a() {

                    }
                    ",
                )?
                .fold_with(&mut OnceMarker::new(&[("a", &[mark1, mark2, mark2])]));
            Ok(stmts)
        },
        "
        const obj = {
            key: function a1() {
                a()
            }
        }
        function a() {

        }
        ",
    );
}

#[test]
fn opt_5() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            let stmts = tester
                .parse_stmts(
                    "actual1.js",
                    "
                    const obj = {
                        a: function a() {
                            a()
                        }
                    }
                    function a() {

                    }
                    ",
                )?
                .fold_with(&mut OnceMarker::new(&[("a", &[mark1, mark2, mark2])]));
            Ok(stmts)
        },
        "
        const obj = {
            a: function a1() {
                a()
            }
        }
        function a() {

        }
        ",
    );
}

#[test]
fn opt_6() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            let stmts = tester
                .parse_stmts(
                    "actual1.js",
                    "
                    var foo = 'bar';
                    var Foo = function() {
                        function Foo() {
                            _bar.set(this, {
                                writable: true,
                                value: foo
                            });
                            var foo = 'foo';
                        }

                    }
                    ",
                )?
                .fold_with(&mut OnceMarker::new(&[("foo", &[mark1, mark1, mark2])]));
            Ok(stmts)
        },
        "
        var foo = 'bar';
        var Foo = function() {
            function Foo() {
                _bar.set(this, {
                    writable: true,
                    value: foo
                });
                var foo1 = 'foo';
            }

        }

        ",
    );
}

#[test]
fn issue_2211_1() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            let stmts = tester
                .parse_stmts(
                    "actual1.js",
                    "
                    var _bar = require('./bar');
                    const makeX = ()=>{
                        const _bar = ()=>(0, _bar).bar()
                        ;
                        return {
                            _bar
                        };
                    };
                    ",
                )?
                .fold_with(&mut OnceMarker::new(&[(
                    "_bar",
                    &[mark1, mark2, mark1, mark2],
                )]));
            Ok(stmts)
        },
        "
        var _bar = require('./bar');
        const makeX = ()=>{
            const _bar1 = ()=>(0, _bar).bar()
            ;
            return {
                _bar: _bar1
            };
        };
        ",
    );
}

#[test]
fn issue_2211_2() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            let stmts = tester
                .parse_stmts(
                    "actual1.js",
                    "
                    var _bar = require('./bar');
                    const makeX = ()=>{
                        const _bar = () => _bar();

                        const alfa = () => _bar();

                        return { alfa };
                    };
                    ",
                )?
                .fold_with(&mut OnceMarker::new(&[(
                    "_bar",
                    &[mark1, mark2, mark1, mark2],
                )]));
            Ok(stmts)
        },
        "
        var _bar = require('./bar');
        const makeX = ()=>{
            const _bar1 = ()=>_bar();
            const alfa = ()=>_bar1();
            return {
                alfa
            };
        };
        ",
    );
}

#[test]
fn issue_2297_1() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            let stmts = tester
                .parse_stmts(
                    "actual1.js",
                    "
                    var _bar = require('./Bar');
                    var makeX = function(props) {
                        var _bar = props.bar;
                        var list = _bar.list;
                        return list.map(function() {
                            return _bar.bar;
                        });
                    };
                    ",
                )?
                .fold_with(&mut OnceMarker::new(&[(
                    "_bar",
                    &[mark1, mark2, mark2, mark1],
                )]));
            Ok(stmts)
        },
        "
        var _bar = require('./Bar');
        var makeX = function(props) {
            var _bar1 = props.bar;
            var list = _bar1.list;
            return list.map(function() {
                return _bar.bar;
            });
        };
        ",
    );
}

/// `var` has strange scoping rule.
#[test]
fn var_awareness_1() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            let stmts = tester
                .parse_stmts(
                    "actual1.js",
                    "
                    for (var i of [1, 2, 3]) {
                        for (var i of [4, 5, 6]) {
                            console.log(i)
                        }
                    }
                    ",
                )?
                .fold_with(&mut OnceMarker::new(&[("i", &[mark1, mark2, mark2])]));
            Ok(stmts)
        },
        "
        for (var i of [1, 2, 3]) {
            for (var i1 of [4, 5, 6]) {
                console.log(i1)
            }
        }
        ",
    );
}

/// `var` has strange scoping rule.
#[test]
fn var_awareness_2() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            let stmts = tester
                .parse_stmts(
                    "actual1.js",
                    "
                    for (var i of [1, 2, 3]) {
                    }
                    for (var i of [4, 5, 6]) {
                        console.log(i)
                    }
                    ",
                )?
                .fold_with(&mut OnceMarker::new(&[("i", &[mark1, mark2, mark2])]));
            Ok(stmts)
        },
        "
        for (var i of [1, 2, 3]) {

        }
        for (var i1 of [4, 5, 6]) {
            console.log(i1)
        }
        ",
    );
}

/// `var` has strange scoping rule.
#[test]
fn issue_2539() {
    test(
        |tester| {
            let mark1 = Mark::fresh(Mark::root());
            let mark2 = Mark::fresh(Mark::root());

            let stmts = tester
                .parse_stmts(
                    "actual1.js",
                    "
                    const obj = {
                        foo: {
                            func1(index) {
                            },
                            func2(index, index1) {
                            },
                        },
                    }
                    ",
                )?
                .fold_with(&mut OnceMarker::new(&[("index", &[mark1, mark2])]));
            Ok(stmts)
        },
        "
        const obj = {
            foo: {
                func1(index) {
                },
                func2(index, index1) {
                },
            },
        }
        ",
    );
}

#[test]
fn rename_keep_contexts() {
    crate::tests::Tester::run(|tester| {
        let marks = vec![
            Mark::root(),
            Mark::fresh(Mark::root()),
            Mark::fresh(Mark::root()),
            Mark::fresh(Mark::root()),
            Mark::fresh(Mark::root()),
        ];

        let mut program = Program::Module(tester.parse_module(
            "actual1.js",
            "
                    const bar$1 = (patch$2)=>patch$2 + 1;
                    const foo$1 = (patch$3)=>patch$3 !== patch$4;
                ",
        )?);

        let expected = "
        const bar$1 = (patch1$7)=>patch1$7 + 1;
        const foo$1 = (patch1$8)=>patch1$8 !== patch$4;
        ";

        program.visit_mut_with(&mut InlineContextMarker { marks });

        struct HygieneRenamer;
        impl Renamer for HygieneRenamer {
            type Target = Id;

            const MANGLE: bool = false;
            const RESET_N: bool = true;

            fn new_name_for(&self, orig: &Id, n: &mut usize) -> swc_atoms::Atom {
                let res = if *n == 0 {
                    orig.0.clone()
                } else {
                    format!("{}{}", orig.0, n).into()
                };
                *n += 1;
                res
            }
        }
        program.visit_mut_with(&mut renamer_keep_contexts(
            Default::default(),
            HygieneRenamer,
        ));
        let program = program.fold_with(&mut HygieneVisualizer {});

        let actual = tester.print(&program);

        let expected = {
            let expected = tester
                .with_parser("expected.js", Syntax::default(), expected, |p| {
                    p.parse_module()
                })
                .map(Program::Module)?;
            tester.print(&expected)
        };

        if actual != expected {
            println!("----- Actual -----\n{actual}");
            println!("----- Diff -----");

            assert_eq!(DebugUsingDisplay(&actual), DebugUsingDisplay(&expected));
        }

        Ok(())
    });
}
