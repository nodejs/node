use std::hint::black_box;

use swc_atoms::atom;
use swc_common::{comments::SingleThreadedComments, BytePos, FileName, SourceMap, DUMMY_SP};
use swc_ecma_visit::assert_eq_ignore_span;

use super::*;
use crate::{parse_file_as_expr, EsSyntax, TsSyntax};

fn program(src: &'static str) -> Program {
    test_parser(src, Default::default(), |p| p.parse_program())
}

/// Assert that Parser.parse_program returns [Program::Module].
fn module(src: &'static str) -> Module {
    program(src).expect_module()
}

/// Assert that Parser.parse_program returns [Program::Script].
fn script(src: &'static str) -> Script {
    program(src).expect_script()
}

/// Assert that Parser.parse_program returns [Program::Module] and has errors.
#[track_caller]
fn assert_module_error(src: &'static str) -> Module {
    test_parser(src, Default::default(), |p| {
        let program = p.parse_program()?;

        let errors = p.take_errors();
        assert_ne!(errors, Vec::new());

        let module = program.expect_module();

        Ok(module)
    })
}

#[test]
fn parse_program_module_01() {
    module("import 'foo';");
    module("export const a = 1;");
}

#[test]
fn parse_program_script_01() {
    script("let a = 5;");
    script("function foo() {}");
    script("const a = 00176;");
}

#[test]
fn parse_program_module_02() {
    module(
        "
        function foo() {}
        export default foo;
        ",
    );
    module(
        "
        export function foo() {}
        export default foo;
        ",
    );
}

#[test]
fn parse_program_module_error_01() {
    assert_module_error(
        "
        const a = 01234;
        export default a;
        ",
    );
}

#[test]
fn issue_1813() {
    test_parser(
        "\\u{cccccccccsccccccQcXt[uc(~).const[uctor().const[uctor())tbr())",
        Default::default(),
        |p| {
            p.parse_program().expect_err("should fail");

            Ok(())
        },
    )
}

#[test]
fn parse_module_export_named_span() {
    let m = module("export function foo() {}");
    if let ModuleItem::ModuleDecl(ModuleDecl::ExportDecl(ExportDecl { span, .. })) = &m.body[0] {
        assert_eq!(span.lo, BytePos(1));
    } else {
        panic!("expected ExportDecl");
    }
}

#[test]
fn parse_module_export_default_fn_span() {
    let m = module("export default function foo() {}");
    if let ModuleItem::ModuleDecl(ModuleDecl::ExportDefaultDecl(ExportDefaultDecl {
        span, ..
    })) = &m.body[0]
    {
        assert_eq!(span.lo, BytePos(1));
        assert_eq!(span.hi, BytePos(33));
    } else {
        panic!("expected ExportDefaultDecl");
    }
}

#[test]
fn parse_module_export_default_async_fn_span() {
    let m = module("export default async function foo() {}");
    if let ModuleItem::ModuleDecl(ModuleDecl::ExportDefaultDecl(ExportDefaultDecl {
        span, ..
    })) = &m.body[0]
    {
        assert_eq!(span.lo, BytePos(1));
        assert_eq!(span.hi, BytePos(39));
    } else {
        panic!("expected ExportDefaultDecl");
    }
}

#[test]
fn parse_module_export_default_class_span() {
    let m = module("export default class Foo {}");
    if let ModuleItem::ModuleDecl(ModuleDecl::ExportDefaultDecl(ExportDefaultDecl {
        span, ..
    })) = &m.body[0]
    {
        assert_eq!(span.lo, BytePos(1));
        assert_eq!(span.hi, BytePos(28));
    } else {
        panic!("expected ExportDefaultDecl");
    }
}

#[test]
fn issue_1878() {
    // file with only comments should have the comments
    // in the leading map instead of the trailing
    {
        let c = SingleThreadedComments::default();
        let s = "
            // test
        ";
        let _ = super::test_parser_comment(&c, s, Syntax::Typescript(Default::default()), |p| {
            p.parse_typescript_module()
        });

        let (leading, trailing) = c.take_all();
        assert!(trailing.borrow().is_empty());
        assert_eq!(leading.borrow().len(), 1);
        assert!(leading.borrow().get(&BytePos(1)).is_some());
    }

    // file with shebang and comments should still work with the comments trailing
    // the shebang
    {
        let c = SingleThreadedComments::default();
        let s = "#!/foo/bar
            // test
        ";
        let _ = super::test_parser_comment(&c, s, Syntax::Typescript(Default::default()), |p| {
            p.parse_typescript_module()
        });

        let (leading, trailing) = c.take_all();
        assert!(leading.borrow().is_empty());
        assert_eq!(trailing.borrow().len(), 1);
        assert!(trailing.borrow().get(&BytePos(11)).is_some());
    }
}

#[test]
fn issue_2264_1() {
    let c = SingleThreadedComments::default();
    let s = "
        const t = <Switch>
            // 1
            /* 2 */
        </Switch>
    ";
    let _ = super::test_parser_comment(
        &c,
        s,
        Syntax::Typescript(TsSyntax {
            tsx: true,
            ..Default::default()
        }),
        |p| p.parse_typescript_module(),
    );

    let (_leading, trailing) = c.take_all();
    // assert!(leading.borrow().is_empty());
    assert!(trailing.borrow().is_empty());
}

#[test]
fn issue_2264_2() {
    let c = SingleThreadedComments::default();
    let s = "
        const t = <Switch>
            // 1
            /* 2 */
        </Switch>
    ";
    let _ = super::test_parser_comment(
        &c,
        s,
        Syntax::Es(EsSyntax {
            jsx: true,
            ..Default::default()
        }),
        |p| p.parse_module(),
    );

    let (leading, trailing) = c.take_all();
    assert!(leading.borrow().is_empty());
    assert!(trailing.borrow().is_empty());
}

#[test]
fn should_only_has_one_block_comment() {
    let c = SingleThreadedComments::default();
    let s = "
/** block comment */
import h from 'h';
<div></div>
";
    let _ = super::test_parser_comment(
        &c,
        s,
        Syntax::Typescript(TsSyntax {
            tsx: true,
            ..Default::default()
        }),
        |p| p.parse_typescript_module(),
    );

    let (leading, trailing) = c.take_all();

    assert!(!leading.borrow().is_empty());
    for leading in leading.borrow().values() {
        assert_eq!(leading.len(), 1);
    }
    assert!(trailing.borrow().is_empty());
}

#[test]
fn issue_2264_3() {
    let c = SingleThreadedComments::default();
    let s = "const foo = <h1>/* no */{/* 1 */ bar /* 2 */}/* no */</h1>;";
    let _ = super::test_parser_comment(
        &c,
        s,
        Syntax::Typescript(TsSyntax {
            tsx: true,
            ..Default::default()
        }),
        |p| p.parse_typescript_module(),
    );

    let (leading, trailing) = c.take_all();

    assert!(leading.borrow().is_empty());
    assert_eq!(trailing.borrow().len(), 2);
    assert_eq!(trailing.borrow().get(&BytePos(26)).unwrap().len(), 1);
    assert_eq!(trailing.borrow().get(&BytePos(37)).unwrap().len(), 1);
}

#[test]
fn issue_2339_1() {
    let c = SingleThreadedComments::default();
    // Use `<T,>` instead of `<T>` because `<T>` is ambiguous with JSX in TSX mode
    let s = "
        const t = <T,>() => {
            // 1
            /* 2 */
            test;
        };
    ";
    let _ = super::test_parser_comment(
        &c,
        s,
        Syntax::Typescript(TsSyntax {
            tsx: true,
            ..Default::default()
        }),
        |p| p.parse_typescript_module(),
    );

    let (leading, trailing) = c.take_all();
    assert_eq!(leading.borrow().len(), 1);
    // Adjusted byte position due to trailing comma in `<T,>`
    assert_eq!(leading.borrow().get(&BytePos(81)).unwrap().len(), 2);
    assert!(trailing.borrow().is_empty());
}

/// Test that `<T>() => {}` is invalid in TSX mode (issue #10598)
/// TypeScript treats `<T>` as JSX in .tsx files, so this syntax should fail.
#[test]
fn issue_10598() {
    // In TSX mode, `<T>() => {}` should fail because `<T>` looks like JSX
    let s = "const t = <T>() => { test; };";
    let result = std::panic::catch_unwind(|| {
        test_parser(
            s,
            Syntax::Typescript(TsSyntax {
                tsx: true,
                ..Default::default()
            }),
            |p| p.parse_typescript_module(),
        )
    });
    // The parsing should fail (panic in test_parser or return an error)
    assert!(
        result.is_err(),
        "Expected parsing to fail for <T>() => {{}} in TSX mode"
    );
}

/// Verify that `<T,>() => {}` is valid in TSX mode (workaround syntax)
#[test]
fn issue_10598_valid_tsx_syntax() {
    // `<T,>` with trailing comma is unambiguous and should parse successfully
    let s = "const t = <T,>() => { test; };";
    test_parser(
        s,
        Syntax::Typescript(TsSyntax {
            tsx: true,
            ..Default::default()
        }),
        |p| p.parse_typescript_module(),
    );
}

/// Verify that `<T extends unknown>() => {}` is valid in TSX mode
#[test]
fn issue_10598_valid_tsx_syntax_with_constraint() {
    // `<T extends unknown>` is unambiguous and should parse successfully
    let s = "const t = <T extends unknown>() => { test; };";
    test_parser(
        s,
        Syntax::Typescript(TsSyntax {
            tsx: true,
            ..Default::default()
        }),
        |p| p.parse_typescript_module(),
    );
}

/// Verify that `<T>() => {}` is still valid in non-TSX TypeScript mode
#[test]
fn issue_10598_valid_non_tsx() {
    // In regular .ts mode (not TSX), `<T>() => {}` should work
    let s = "const t = <T>() => { test; };";
    test_parser(
        s,
        Syntax::Typescript(TsSyntax {
            tsx: false,
            ..Default::default()
        }),
        |p| p.parse_typescript_module(),
    );
}

#[test]
fn issue_2853_1() {
    test_parser("const a = \"\\0a\";", Default::default(), |p| {
        let program = p.parse_program()?;

        let errors = p.take_errors();
        assert_eq!(errors, Vec::new());
        assert_eq!(errors, Vec::new());

        Ok(program)
    });
}

#[test]
fn issue_2853_2() {
    test_parser("const a = \"\u{0000}a\";", Default::default(), |p| {
        let program = p.parse_program()?;

        let errors = p.take_errors();
        assert_eq!(errors, Vec::new());

        Ok(program)
    });
}

#[test]
fn illegal_language_mode_directive1() {
    test_parser(
        r#"function f(a = 0) { "use strict"; }"#,
        Default::default(),
        |p| {
            let program = p.parse_program()?;

            let errors = p.take_errors();
            assert_eq!(
                errors,
                vec![Error::new(
                    Span {
                        lo: BytePos(21),
                        hi: BytePos(34),
                    },
                    SyntaxError::IllegalLanguageModeDirective
                )]
            );

            Ok(program)
        },
    );
}

#[test]
fn illegal_language_mode_directive2() {
    test_parser(
        r#"let f = (a = 0) => { "use strict"; }"#,
        Default::default(),
        |p| {
            let program = p.parse_program()?;

            let errors = p.take_errors();
            assert_eq!(
                errors,
                vec![Error::new(
                    Span {
                        lo: BytePos(22),
                        hi: BytePos(35),
                    },
                    SyntaxError::IllegalLanguageModeDirective
                )]
            );

            Ok(program)
        },
    );
}

#[test]
fn parse_non_strict_for_loop() {
    script("for (var v1 = 1 in v3) {}");
}

#[test]
fn parse_program_take_script_module_errors() {
    test_parser(r#"077;"#, Default::default(), |p| {
        let program = p.parse_program()?;

        assert_eq!(p.take_errors(), vec![]);
        // will contain the script's potential module errors
        assert_eq!(
            p.take_script_module_errors(),
            vec![Error::new(
                Span {
                    lo: BytePos(1),
                    hi: BytePos(4),
                },
                SyntaxError::LegacyOctal
            )]
        );

        Ok(program)
    });
}

fn syntax() -> Syntax {
    Syntax::Es(EsSyntax {
        allow_super_outside_method: true,
        ..Default::default()
    })
}

fn lhs(s: &'static str) -> Box<Expr> {
    test_parser(s, syntax(), |p| p.parse_lhs_expr())
}

fn new_expr(s: &'static str) -> Box<Expr> {
    test_parser(s, syntax(), |p| p.parse_new_expr())
}

fn member_expr(s: &'static str) -> Box<Expr> {
    test_parser(s, syntax(), |p| p.parse_member_expr())
}

fn expr(s: &'static str) -> Box<Expr> {
    test_parser(s, syntax(), |p| {
        p.parse_stmt().map(|stmt| match stmt {
            Stmt::Expr(expr) => expr.expr,
            _ => unreachable!(),
        })
    })
}
fn regex_expr() -> Box<Expr> {
    AssignExpr {
        span: DUMMY_SP,
        left: Ident::new_no_ctxt(atom!("re"), DUMMY_SP).into(),
        op: AssignOp::Assign,
        right: Box::new(
            Lit::Regex(Regex {
                span: DUMMY_SP,
                exp: atom!("w+"),
                flags: atom!(""),
            })
            .into(),
        ),
    }
    .into()
}

fn bin(s: &'static str) -> Box<Expr> {
    test_parser(s, Syntax::default(), |p| p.parse_bin_expr())
}

#[test]
fn simple() {
    assert_eq_ignore_span!(
        bin("5 + 4 * 7"),
        Box::new(Expr::Bin(BinExpr {
            span: DUMMY_SP,
            op: op!(bin, "+"),
            left: bin("5"),
            right: bin("4 * 7"),
        }))
    );
}

#[test]
fn same_prec() {
    assert_eq_ignore_span!(
        bin("5 + 4 + 7"),
        Box::new(Expr::Bin(BinExpr {
            span: DUMMY_SP,
            op: op!(bin, "+"),
            left: bin("5 + 4"),
            right: bin("7"),
        }))
    );
}

#[test]
fn regex_single_line_comment() {
    assert_eq_ignore_span!(
        expr(
            r#"re = // ...
            /w+/"#
        ),
        regex_expr()
    )
}

#[test]
fn regex_multi_line_comment() {
    assert_eq_ignore_span!(expr(r#"re = /* ... *//w+/"#), regex_expr())
}
#[test]
fn regex_multi_line_comment_with_lines() {
    assert_eq_ignore_span!(
        expr(
            r#"re =
            /*
             ...
             */
             /w+/"#
        ),
        regex_expr()
    )
}

#[test]
fn arrow_assign() {
    assert_eq_ignore_span!(
        expr("a = b => false"),
        Box::new(Expr::Assign(AssignExpr {
            span: DUMMY_SP,
            left: Ident::new_no_ctxt(atom!("a"), DUMMY_SP).into(),
            op: op!("="),
            right: expr("b => false"),
        }))
    );
}

#[test]
fn async_call() {
    assert_eq_ignore_span!(
        expr("async()"),
        Box::new(Expr::Call(CallExpr {
            span: DUMMY_SP,
            callee: Callee::Expr(expr("async")),
            args: Vec::new(),
            ..Default::default()
        }))
    );
}

#[test]
fn async_arrow() {
    assert_eq_ignore_span!(
        expr("async () => foo"),
        Box::new(Expr::Arrow(ArrowExpr {
            span: DUMMY_SP,
            is_async: true,
            is_generator: false,
            params: Vec::new(),
            body: Box::new(BlockStmtOrExpr::Expr(expr("foo"))),
            ..Default::default()
        }))
    );
}

#[test]
fn object_rest_pat() {
    assert_eq_ignore_span!(
        expr("({ ...a34 }) => {}"),
        Box::new(Expr::Arrow(ArrowExpr {
            span: DUMMY_SP,
            is_async: false,
            is_generator: false,
            params: vec![Pat::Object(ObjectPat {
                span: DUMMY_SP,
                optional: false,
                props: vec![ObjectPatProp::Rest(RestPat {
                    span: DUMMY_SP,
                    dot3_token: DUMMY_SP,
                    arg: Box::new(Pat::Ident(
                        Ident::new_no_ctxt(atom!("a34"), DUMMY_SP).into()
                    )),
                    type_ann: None,
                })],
                type_ann: None
            })],
            body: Box::new(BlockStmtOrExpr::BlockStmt(BlockStmt {
                span: DUMMY_SP,
                ..Default::default()
            })),
            ..Default::default()
        }))
    );
}

#[test]
fn object_spread() {
    assert_eq_ignore_span!(
        expr("foo = {a, ...bar, b}"),
        Box::new(Expr::Assign(AssignExpr {
            span: DUMMY_SP,
            left: Ident::new_no_ctxt(atom!("foo"), DUMMY_SP).into(),
            op: op!("="),
            right: Box::new(Expr::Object(ObjectLit {
                span: DUMMY_SP,
                props: vec![
                    PropOrSpread::Prop(Box::new(Ident::new_no_ctxt(atom!("a"), DUMMY_SP).into())),
                    PropOrSpread::Spread(SpreadElement {
                        dot3_token: DUMMY_SP,
                        expr: Box::new(Expr::Ident(Ident::new_no_ctxt(atom!("bar"), DUMMY_SP))),
                    }),
                    PropOrSpread::Prop(Box::new(Ident::new_no_ctxt(atom!("b"), DUMMY_SP).into())),
                ]
            }))
        }))
    );
}

#[test]
fn new_expr_should_not_eat_too_much() {
    assert_eq_ignore_span!(
        new_expr("new Date().toString()"),
        Box::new(Expr::Member(MemberExpr {
            span: DUMMY_SP,
            obj: member_expr("new Date()"),
            prop: MemberProp::Ident(IdentName::new(atom!("toString"), DUMMY_SP)),
        }))
    );
}
#[test]
fn lhs_expr_as_new_expr_prod() {
    assert_eq_ignore_span!(
        lhs("new Date.toString()"),
        Box::new(Expr::New(NewExpr {
            span: DUMMY_SP,
            callee: lhs("Date.toString"),
            args: Some(Vec::new()),
            ..Default::default()
        }))
    );
}

#[test]
fn lhs_expr_as_call() {
    assert_eq_ignore_span!(
        lhs("new Date.toString()()"),
        Box::new(Expr::Call(CallExpr {
            span: DUMMY_SP,
            callee: Callee::Expr(lhs("new Date.toString()")),
            args: Vec::new(),
            ..Default::default()
        }))
    )
}

#[test]
fn arrow_fn_no_args() {
    assert_eq_ignore_span!(
        expr("() => 1"),
        Box::new(Expr::Arrow(ArrowExpr {
            span: DUMMY_SP,
            is_async: false,
            is_generator: false,
            params: Vec::new(),
            body: Box::new(BlockStmtOrExpr::Expr(expr("1"))),
            ..Default::default()
        }))
    );
}
#[test]
fn arrow_fn() {
    assert_eq_ignore_span!(
        expr("(a) => 1"),
        Box::new(Expr::Arrow(ArrowExpr {
            span: DUMMY_SP,
            is_async: false,
            is_generator: false,
            params: vec![Pat::Ident(Ident::new_no_ctxt(atom!("a"), DUMMY_SP).into())],
            body: Box::new(BlockStmtOrExpr::Expr(expr("1"))),
            ..Default::default()
        }))
    );
}
#[test]
fn arrow_fn_rest() {
    assert_eq_ignore_span!(
        expr("(...a) => 1"),
        Box::new(Expr::Arrow(ArrowExpr {
            span: DUMMY_SP,
            is_async: false,
            is_generator: false,
            params: vec![Pat::Rest(RestPat {
                span: DUMMY_SP,
                dot3_token: DUMMY_SP,
                arg: Box::new(Pat::Ident(Ident::new_no_ctxt(atom!("a"), DUMMY_SP).into())),
                type_ann: None
            })],
            body: Box::new(BlockStmtOrExpr::Expr(expr("1"))),

            ..Default::default()
        }))
    );
}
#[test]
fn arrow_fn_no_paren() {
    assert_eq_ignore_span!(
        expr("a => 1"),
        Box::new(Expr::Arrow(ArrowExpr {
            span: DUMMY_SP,
            params: vec![Pat::Ident(Ident::new_no_ctxt(atom!("a"), DUMMY_SP).into())],
            body: Box::new(BlockStmtOrExpr::Expr(expr("1"))),
            ..Default::default()
        }))
    );
}

#[test]
fn new_no_paren() {
    assert_eq_ignore_span!(
        expr("new a"),
        Box::new(Expr::New(NewExpr {
            span: DUMMY_SP,
            callee: expr("a"),
            args: None,
            ..Default::default()
        }))
    );
}

#[test]
fn new_new_no_paren() {
    assert_eq_ignore_span!(
        expr("new new a"),
        Box::new(Expr::New(NewExpr {
            span: DUMMY_SP,
            callee: expr("new a"),
            args: None,
            ..Default::default()
        }))
    );
}

#[test]
fn array_lit() {
    assert_eq_ignore_span!(
        expr("[a,,,,, ...d,, e]"),
        Box::new(Expr::Array(ArrayLit {
            span: DUMMY_SP,
            elems: vec![
                Some(ExprOrSpread {
                    spread: None,
                    expr: Box::new(Expr::Ident(Ident::new_no_ctxt(atom!("a"), DUMMY_SP))),
                }),
                None,
                None,
                None,
                None,
                Some(ExprOrSpread {
                    spread: Some(DUMMY_SP),
                    expr: Box::new(Expr::Ident(Ident::new_no_ctxt(atom!("d"), DUMMY_SP))),
                }),
                None,
                Some(ExprOrSpread {
                    spread: None,
                    expr: Box::new(Expr::Ident(Ident::new_no_ctxt(atom!("e"), DUMMY_SP))),
                }),
            ]
        }))
    );
}

#[test]
fn max_integer() {
    assert_eq_ignore_span!(
        expr("1.7976931348623157e+308"),
        Box::new(Expr::Lit(Lit::Num(Number {
            span: DUMMY_SP,
            value: 1.797_693_134_862_315_7e308,
            raw: Some(atom!("1.7976931348623157e+308")),
        })))
    )
}

#[test]
fn iife() {
    assert_eq_ignore_span!(
        expr("(function(){})()"),
        Box::new(Expr::Call(CallExpr {
            span: DUMMY_SP,
            callee: Callee::Expr(expr("(function(){})")),
            args: Vec::new(),
            ..Default::default()
        }))
    )
}

#[test]
fn issue_319_1() {
    assert_eq_ignore_span!(
        expr("obj(({ async f() { await g(); } }));"),
        Box::new(Expr::Call(CallExpr {
            span: DUMMY_SP,
            callee: Callee::Expr(expr("obj")),
            args: vec![ExprOrSpread {
                spread: None,
                expr: expr("({ async f() { await g(); } })"),
            }],
            ..Default::default()
        }))
    );
}

#[test]
fn issue_328() {
    assert_eq_ignore_span!(
        test_parser("import('test')", Syntax::Es(Default::default()), |p| {
            p.parse_stmt()
        }),
        Stmt::Expr(ExprStmt {
            span: DUMMY_SP,
            expr: Box::new(Expr::Call(CallExpr {
                span: DUMMY_SP,
                callee: Callee::Import(Import {
                    span: DUMMY_SP,
                    phase: Default::default()
                }),
                args: vec![ExprOrSpread {
                    spread: None,
                    expr: Box::new(Expr::Lit(Lit::Str(Str {
                        span: DUMMY_SP,
                        value: atom!("test").into(),
                        raw: Some(atom!("'test'")),
                    }))),
                }],
                ..Default::default()
            }))
        })
    );
}

#[test]
fn issue_337() {
    test_parser(
        "const foo = 'bar' in bas ? 'beep' : 'boop';",
        Default::default(),
        |p| p.parse_module(),
    );
}

#[test]
fn issue_350() {
    assert_eq_ignore_span!(
        expr(
            r#""ok\
ok\
hehe.";"#,
        ),
        Box::new(Expr::Lit(Lit::Str(Str {
            span: DUMMY_SP,
            value: atom!("okokhehe.").into(),
            raw: Some(atom!("\"ok\\\nok\\\nhehe.\"")),
        })))
    );
}

#[test]
fn issue_380() {
    expr(
        " import('../foo/bar')
    .then(bar => {
        // bar should be {default: DEFAULT_EXPORTED_THING_IN_BAR} or at least what it is supposed \
         to be
    })
}",
    );
}

#[test]
fn issue_675() {
    expr("fn = function () { Object.setPrototypeOf(this, new.target.prototype); }");
}

#[test]
fn super_expr() {
    assert_eq_ignore_span!(
        expr("super.foo();"),
        Box::new(Expr::Call(CallExpr {
            span: DUMMY_SP,
            callee: Callee::Expr(Box::new(Expr::SuperProp(SuperPropExpr {
                span: DUMMY_SP,
                obj: Super { span: DUMMY_SP },
                prop: SuperProp::Ident(IdentName {
                    span: DUMMY_SP,
                    sym: atom!("foo"),
                })
            }))),
            ..Default::default()
        }))
    );
}

#[test]
fn super_expr_computed() {
    assert_eq_ignore_span!(
        expr("super[a] ??= 123;"),
        Box::new(Expr::Assign(AssignExpr {
            span: DUMMY_SP,
            op: AssignOp::NullishAssign,
            left: SuperPropExpr {
                span: DUMMY_SP,
                obj: Super { span: DUMMY_SP },
                prop: SuperProp::Computed(ComputedPropName {
                    span: DUMMY_SP,
                    expr: Box::new(Expr::Ident(Ident {
                        span: DUMMY_SP,
                        sym: atom!("a"),
                        ..Default::default()
                    })),
                })
            }
            .into(),
            right: Box::new(Expr::Lit(Lit::Num(Number {
                span: DUMMY_SP,
                value: 123f64,
                raw: Some(atom!("123")),
            })))
        }))
    );
}

#[test]
fn issue_3672_1() {
    test_parser(
        "report({
    fix: fixable ? null : (): RuleFix => {},
});",
        Syntax::Typescript(Default::default()),
        |p| p.parse_module(),
    );
}

#[test]
fn issue_3672_2() {
    test_parser(
        "f(a ? (): void => { } : (): void => { })",
        Syntax::Typescript(Default::default()),
        |p| p.parse_module(),
    );
}

#[test]
fn issue_5947() {
    test_parser(
        "[a as number, b as number, c as string] = [1, 2, '3']",
        Syntax::Typescript(Default::default()),
        |p| p.parse_module(),
    );
}

#[test]
fn issue_6781() {
    let cm = SourceMap::default();
    let fm = cm.new_source_file(FileName::Anon.into(), "import.meta.env".to_string());
    let mut errors = Vec::new();
    let expr = parse_file_as_expr(
        &fm,
        Default::default(),
        Default::default(),
        None,
        &mut errors,
    );
    assert!(expr.is_ok());
    assert!(errors.is_empty());
}

#[bench]
fn bench_new_expr_ts(b: &mut Bencher) {
    bench_parser(
        b,
        "new Foo()",
        Syntax::Typescript(Default::default()),
        |p| {
            black_box(p.parse_expr()?);
            Ok(())
        },
    );
}

#[bench]
fn bench_new_expr_es(b: &mut Bencher) {
    bench_parser(b, "new Foo()", Syntax::Es(Default::default()), |p| {
        black_box(p.parse_expr()?);
        Ok(())
    });
}

#[bench]
fn bench_member_expr_ts(b: &mut Bencher) {
    bench_parser(
        b,
        "a.b.c.d.e.f",
        Syntax::Typescript(Default::default()),
        |p| {
            black_box(p.parse_expr()?);
            Ok(())
        },
    );
}

#[bench]
fn bench_member_expr_es(b: &mut Bencher) {
    bench_parser(b, "a.b.c.d.e.f", Syntax::Es(Default::default()), |p| {
        black_box(p.parse_expr()?);
        Ok(())
    });
}
