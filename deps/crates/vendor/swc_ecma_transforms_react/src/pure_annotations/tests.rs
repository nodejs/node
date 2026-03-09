use swc_common::{comments::SingleThreadedComments, sync::Lrc, FileName, Mark, SourceMap};
use swc_ecma_codegen::{text_writer::JsWriter, Emitter};
use swc_ecma_parser::{Parser, StringInput};
use swc_ecma_transforms_base::resolver;
use swc_ecma_transforms_testing::Tester;

use super::*;

fn parse(
    tester: &mut Tester,
    src: &str,
) -> Result<(Program, Lrc<SourceMap>, Lrc<SingleThreadedComments>), ()> {
    let syntax = ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    });
    let source_map = Lrc::new(SourceMap::default());
    let source_file = source_map.new_source_file(FileName::Anon.into(), src.to_string());

    let comments = Lrc::new(SingleThreadedComments::default());
    let program = {
        let mut p = Parser::new(syntax, StringInput::from(&*source_file), Some(&comments));
        let res = p
            .parse_module()
            .map_err(|e| e.into_diagnostic(tester.handler).emit());

        for e in p.take_errors() {
            e.into_diagnostic(tester.handler).emit()
        }

        Program::Module(res?)
    };

    Ok((program, source_map, comments))
}

fn emit(
    source_map: Lrc<SourceMap>,
    comments: Lrc<SingleThreadedComments>,
    program: &Program,
) -> String {
    let mut src_map_buf = Vec::new();
    let mut buf = std::vec::Vec::new();
    {
        let writer = Box::new(JsWriter::new(
            source_map.clone(),
            "\n",
            &mut buf,
            Some(&mut src_map_buf),
        ));
        let mut emitter = Emitter {
            cfg: Default::default(),
            comments: Some(&comments),
            cm: source_map,
            wr: writer,
        };
        emitter.emit_program(program).unwrap();
    }

    String::from_utf8(buf).unwrap()
}

fn run_test(input: &str, expected: &str) {
    Tester::run(|tester| {
        let unresolved_mark = Mark::new();
        let top_level_mark = Mark::new();

        let (actual, actual_sm, actual_comments) = parse(tester, input)?;
        let actual = actual
            .apply(&mut resolver(unresolved_mark, top_level_mark, false))
            .apply(&mut crate::react(
                actual_sm.clone(),
                Some(&actual_comments),
                Default::default(),
                top_level_mark,
                unresolved_mark,
            ));

        let actual_src = emit(actual_sm, actual_comments, &actual);

        let (expected, expected_sm, expected_comments) = parse(tester, expected)?;
        let expected_src = emit(expected_sm, expected_comments, &expected);

        if actual_src != expected_src {
            println!(">>>>> Orig <<<<<\n{input}");
            println!(">>>>> Code <<<<<\n{actual_src}");
            panic!(
                r#"assertion failed: `(left == right)`
    {}"#,
                ::testing::diff(&actual_src, &expected_src),
            );
        }

        Ok(())
    });
}

macro_rules! test {
    ($test_name:ident, $input:expr, $expected:expr) => {
        #[test]
        fn $test_name() {
            run_test($input, $expected)
        }
    };
}

test!(
    forward_ref,
    r#"
  import {forwardRef} from 'react';
  const Comp = forwardRef((props, ref) => null);
  "#,
    r#"
  import {forwardRef} from 'react';
  const Comp = /*#__PURE__*/ forwardRef((props, ref) => null);
  "#
);

test!(
    create_element,
    r#"
  import React from 'react';
  React.createElement('div');
  "#,
    r#"
  import React from 'react';
  /*#__PURE__*/ React.createElement('div');
  "#
);

test!(
    create_element_jsx,
    r#"
  import React from 'react';
  const x = <div />;
  "#,
    r#"
  import React from 'react';
  const x = /*#__PURE__*/ React.createElement("div", null);
  "#
);

test!(
    create_element_fragment_jsx,
    r#"
  import React from 'react';
  const x = <><div /></>;
  "#,
    r#"
  import React from 'react';
  const x = /*#__PURE__*/ React.createElement(React.Fragment, null, /*#__PURE__*/ React.createElement("div", null));
  "#
);

test!(
    clone_element,
    r#"
  import React from 'react';
  React.cloneElement(React.createElement('div'));
  "#,
    r#"
  import React from 'react';
  /*#__PURE__*/ React.cloneElement(/*#__PURE__*/ React.createElement('div'));
  "#
);

test!(
    create_context,
    r#"
  import { createContext } from 'react';
  const context = createContext({});
  "#,
    r#"
  import { createContext } from 'react';
  const context = /*#__PURE__*/createContext({});
  "#
);

test!(
    create_factory,
    r#"
  import {createFactory} from 'react';
  const div = createFactory('div');
  "#,
    r#"
  import { createFactory } from 'react';
  const div = /*#__PURE__*/createFactory('div');
  "#
);

test!(
    create_ref,
    r#"
  import React from 'react';
  React.createRef();
  "#,
    r#"
  import React from 'react';
  /*#__PURE__*/ React.createRef();
  "#
);

test!(
    is_valid_element,
    r#"
  import React from 'react';
  const isElement = React.isValidElement(React.createElement('div'));
  "#,
    r#"
  import React from 'react';
  const isElement = /*#__PURE__*/React.isValidElement( /*#__PURE__*/React.createElement('div'));
  "#
);

test!(
    lazy,
    r#"
  import React from 'react';
  const SomeComponent = React.lazy(() => import('./SomeComponent'));
  "#,
    r#"
  import React from 'react';
  const SomeComponent = /*#__PURE__*/React.lazy(() => import('./SomeComponent'));
  "#
);

test!(
    memo,
    r#"
  import React from 'react';
  const Comp = React.memo((props) => null);
  "#,
    r#"
  import React from 'react';
  const Comp = /*#__PURE__*/React.memo(props => null);
  "#
);

test!(
    create_portal,
    r#"
  import * as React from 'react';
  import ReactDOM from 'react-dom';
  const Portal = ReactDOM.createPortal(React.createElement('div'), document.getElementById('test'));
  "#,
    r#"
  import * as React from 'react';
  import ReactDOM from 'react-dom';
  const Portal = /*#__PURE__*/ReactDOM.createPortal( /*#__PURE__*/React.createElement('div'), document.getElementById('test'));
  "#
);

test!(
    non_pure_react,
    r#"
  import {useState} from 'react';
  useState(2);
  "#,
    r#"
  import {useState} from 'react';
  useState(2);
  "#
);

test!(
    non_pure_react_dom,
    r#"
  import React from 'react';
  import ReactDOM from 'react-dom';
  ReactDOM.render(React.createElement('div'));
  "#,
    r#"
  import React from 'react';
  import ReactDOM from 'react-dom';
  ReactDOM.render(/*#__PURE__*/React.createElement('div'));
  "#
);

test!(
    non_react_named,
    r#"
  import {createElement} from 'foo';
  createElement('hi');
  "#,
    r#"
  import {createElement} from 'foo';
  createElement('hi');
  "#
);

test!(
    non_react_namespace,
    r#"
  import * as foo from 'foo';
  foo.createElement('hi');
  "#,
    r#"
  import * as foo from 'foo';
  foo.createElement('hi');
  "#
);

test!(
    non_react_default,
    r#"
  import foo from 'foo';
  foo.createElement('hi');
  "#,
    r#"
  import foo from 'foo';
  foo.createElement('hi');
  "#
);
