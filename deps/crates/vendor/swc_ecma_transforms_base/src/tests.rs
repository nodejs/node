use swc_common::{
    comments::SingleThreadedComments,
    errors::{Handler, HANDLER},
    sync::Lrc,
    FileName, SourceMap,
};
use swc_ecma_ast::*;
use swc_ecma_codegen::Emitter;
use swc_ecma_parser::{error::Error, lexer::Lexer, Parser, StringInput, Syntax};
use swc_ecma_utils::DropSpan;
use swc_ecma_visit::{Fold, FoldWith};

use crate::{fixer::fixer, helpers::HELPERS, hygiene::hygiene_with_config};

pub struct Tester<'a> {
    pub cm: Lrc<SourceMap>,
    pub handler: &'a Handler,
    pub comments: Lrc<SingleThreadedComments>,
}

impl Tester<'_> {
    pub fn run<F>(op: F)
    where
        F: FnOnce(&mut Tester<'_>) -> Result<(), ()>,
    {
        let out = ::testing::run_test(false, |cm, handler| {
            HANDLER.set(handler, || {
                HELPERS.set(&Default::default(), || {
                    op(&mut Tester {
                        cm,
                        handler,
                        comments: Default::default(),
                    })
                })
            })
        });

        match out {
            Ok(()) => {}
            Err(stderr) => panic!("Stderr:\n{stderr}"),
        }
    }

    pub fn with_parser<F, T>(
        &mut self,
        file_name: &str,
        syntax: Syntax,
        src: &str,
        op: F,
    ) -> Result<T, ()>
    where
        F: FnOnce(&mut Parser<Lexer>) -> Result<T, Error>,
    {
        let fm = self
            .cm
            .new_source_file(FileName::Real(file_name.into()).into(), src.to_string());

        let mut p = Parser::new(syntax, StringInput::from(&*fm), Some(&self.comments));
        let res = op(&mut p).map_err(|e| e.into_diagnostic(self.handler).emit());

        for e in p.take_errors() {
            e.into_diagnostic(self.handler).emit()
        }

        res
    }

    pub fn parse_module(&mut self, file_name: &str, src: &str) -> Result<Module, ()> {
        self.with_parser(file_name, Syntax::default(), src, |p| p.parse_module())
    }

    pub fn parse_stmts(&mut self, file_name: &str, src: &str) -> Result<Vec<Stmt>, ()> {
        let stmts = self.with_parser(file_name, Syntax::default(), src, |p| {
            p.parse_script().map(|script| script.body)
        })?;

        Ok(stmts)
    }

    pub fn parse_stmt(&mut self, file_name: &str, src: &str) -> Result<Stmt, ()> {
        let mut stmts = self.parse_stmts(file_name, src)?;
        assert!(stmts.len() == 1);

        Ok(stmts.pop().unwrap())
    }

    pub fn apply_transform<T: Pass>(
        &mut self,
        tr: T,
        name: &str,
        syntax: Syntax,
        src: &str,
    ) -> Result<Program, ()> {
        let fm = self
            .cm
            .new_source_file(FileName::Real(name.into()).into(), src.to_string());

        let module = {
            let mut p = Parser::new(syntax, StringInput::from(&*fm), Some(&self.comments));
            let res = p
                .parse_module()
                .map_err(|e| e.into_diagnostic(self.handler).emit());

            for e in p.take_errors() {
                e.into_diagnostic(self.handler).emit()
            }

            res?
        };

        let module = Program::Module(module).apply(tr).apply(DropSpan);

        Ok(module)
    }

    pub fn print(&mut self, program: &Program) -> String {
        let mut buf = Vec::new();
        {
            let mut emitter = Emitter {
                cfg: Default::default(),
                cm: self.cm.clone(),
                wr: Box::new(swc_ecma_codegen::text_writer::JsWriter::new(
                    self.cm.clone(),
                    "\n",
                    &mut buf,
                    None,
                )),
                comments: None,
            };

            // println!("Emitting: {:?}", module);
            emitter.emit_program(program).unwrap();
        }

        let s = String::from_utf8_lossy(&buf);
        s.to_string()
    }
}

pub(crate) struct HygieneVisualizer;
impl Fold for HygieneVisualizer {
    fn fold_ident(&mut self, ident: Ident) -> Ident {
        Ident {
            sym: format!("{}${}", ident.sym, ident.ctxt.as_u32()).into(),
            ..ident
        }
    }
}

pub(crate) fn test_transform<F, P>(
    syntax: Syntax,
    tr: F,
    input: &str,
    expected: &str,
    ok_if_code_eq: bool,
    hygiene_config: impl FnOnce() -> crate::hygiene::Config,
) where
    F: FnOnce(&mut Tester) -> P,
    P: Pass,
{
    crate::tests::Tester::run(|tester| {
        let expected = tester.apply_transform(DropSpan, "output.js", syntax, expected)?;

        println!("----- Actual -----");

        let tr = tr(tester);
        let actual = tester.apply_transform(tr, "input.js", syntax, input)?;

        match ::std::env::var("PRINT_HYGIENE") {
            Ok(ref s) if s == "1" => {
                let hygiene_src = tester.print(&actual.clone().fold_with(&mut HygieneVisualizer));
                println!("----- Hygiene -----\n{hygiene_src}");
            }
            _ => {}
        }

        let actual = actual
            .apply(hygiene_with_config(hygiene_config()))
            .apply(fixer(None))
            .apply(DropSpan);

        if actual == expected {
            return Ok(());
        }

        let (actual_src, expected_src) = (tester.print(&actual), tester.print(&expected));

        if actual_src == expected_src {
            if ok_if_code_eq {
                return Ok(());
            }
            // Diff it
            println!(">>>>> Code <<<<<\n{actual_src}");
            assert_eq!(actual, expected, "different ast was detected");
            return Err(());
        }

        println!(">>>>> Orig <<<<<\n{input}");
        println!(">>>>> Code <<<<<\n{actual_src}");
        if actual_src != expected_src {
            panic!(
                r#"assertion failed: `(left == right)`
            {}"#,
                ::testing::diff(&actual_src, &expected_src),
            );
        }

        Err(())
    });
}
