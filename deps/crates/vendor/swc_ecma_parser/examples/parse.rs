use std::{ffi::OsStr, path::Path};

use swc_common::{
    errors::{ColorConfig, Handler},
    sync::Lrc,
    SourceMap,
};
use swc_ecma_ast::Program;
use swc_ecma_parser::{lexer::Lexer, EsSyntax, Parser, StringInput, Syntax, TsSyntax};

fn main() {
    let cm: Lrc<SourceMap> = Default::default();
    let handler = Handler::with_tty_emitter(ColorConfig::Auto, true, false, Some(cm.clone()));

    // read file path from command line argument or use a default
    let file_path = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "test.js".to_string());

    let file_path = Path::new(&file_path);

    let ext = file_path.extension().map(OsStr::to_ascii_lowercase);
    let is_ts = ext.as_ref().is_some_and(|e| {
        e == "ts" || e == "tsx" || e == "mts" || e == "cts" || e == "mtsx" || e == "ctsx"
    });
    let is_d_ts = is_ts
        && file_path
            .file_name()
            .and_then(OsStr::to_str)
            .is_some_and(|s| {
                let mut iter = s.rsplit('.');

                if iter.next() != Some("ts") {
                    return false;
                }

                iter.next() == Some("d") || iter.next() == Some("d")
            });

    let is_jsx = ext
        .as_ref()
        .is_some_and(|e| e == "jsx" || e == "tsx" || e == "mjsx" || e == "cjsx");
    let is_esm = ext
        .as_ref()
        .is_some_and(|e| e == "mjs" || e == "mts" || e == "mjsx" || e == "mtsx");
    let is_commonjs = ext
        .as_ref()
        .is_some_and(|e| e == "cjs" || e == "cts" || e == "cjsx" || e == "ctsx");

    let fm = cm.load_file(file_path).expect("failed to load test.ts");

    let syntax = if is_ts {
        Syntax::Typescript(TsSyntax {
            tsx: is_jsx,
            dts: is_d_ts,
            ..Default::default()
        })
    } else {
        Syntax::Es(EsSyntax {
            jsx: is_jsx,
            ..Default::default()
        })
    };

    let lexer = Lexer::new(syntax, Default::default(), StringInput::from(&*fm), None);

    let mut parser = Parser::new_from(lexer);

    let program = if is_esm {
        parser
            .parse_module()
            .map(Program::Module)
            .map_err(|e| e.into_diagnostic(&handler).emit())
    } else if is_commonjs {
        parser
            .parse_commonjs()
            .map(Program::Script)
            .map_err(|e| e.into_diagnostic(&handler).emit())
    } else {
        parser
            .parse_program()
            .map_err(|e| e.into_diagnostic(&handler).emit())
    };

    for e in parser.take_errors() {
        e.into_diagnostic(&handler).emit();
    }

    let program = program.expect("Failed to parse program.");

    eprintln!("Parsed program:\n {program:#?}");
}
