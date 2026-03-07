#![allow(dead_code)]

use std::{
    fs,
    path::{Path, PathBuf},
};

use swc_ecma_codegen::{Config, Emitter};
use swc_ecma_parser::{EsSyntax, Parser, StringInput};
use swc_ecma_transforms_base::{fixer::fixer, hygiene, resolver};
use swc_ecma_transforms_compat::es2015::{arrow, classes};
#[cfg(feature = "es3")]
use swc_ecma_transforms_compat::es3::property_literals;
use swc_ecma_transforms_testing::{parse_options, test, test_fixture, FixtureTestConfig, Tester};
use testing::NormalizedOutput;

use super::*;
use crate::{display_name, pure_annotations, react};

fn tr(t: &mut Tester, options: Options, top_level_mark: Mark) -> impl Pass {
    let unresolved_mark = Mark::new();

    (
        resolver(unresolved_mark, top_level_mark, false),
        jsx(
            t.cm.clone(),
            Some(t.comments.clone()),
            options,
            top_level_mark,
            unresolved_mark,
        ),
        display_name(),
        classes(Default::default()),
        arrow(unresolved_mark),
    )
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
struct FixtureOptions {
    #[serde(flatten)]
    options: Options,

    #[serde(default, rename = "BABEL_8_BREAKING")]
    babel_8_breaking: bool,

    #[serde(default = "true_by_default")]
    pure: bool,

    #[serde(default)]
    throws: Option<String>,

    #[serde(default, alias = "useBuiltIns")]
    use_builtins: bool,
}

fn true_by_default() -> bool {
    true
}

fn fixture_tr(t: &mut Tester, mut options: FixtureOptions) -> impl Pass {
    let unresolved_mark = Mark::new();
    let top_level_mark = Mark::new();

    options.options.next = Some(options.babel_8_breaking || options.options.runtime.is_some());

    if !options.babel_8_breaking && options.options.runtime.is_none() {
        options.options.runtime = Some(Runtime::Classic);
    }

    (
        resolver(unresolved_mark, top_level_mark, false),
        jsx(
            t.cm.clone(),
            Some(t.comments.clone()),
            options.options,
            top_level_mark,
            unresolved_mark,
        ),
        display_name(),
        pure_annotations(Some(t.comments.clone())),
    )
}

fn integration_tr(t: &mut Tester, mut options: FixtureOptions) -> impl Pass {
    let unresolved_mark = Mark::new();
    let top_level_mark = Mark::new();

    options.options.next = Some(options.babel_8_breaking || options.options.runtime.is_some());

    if !options.babel_8_breaking && options.options.runtime.is_none() {
        options.options.runtime = Some(Runtime::Classic);
    }

    (
        resolver(unresolved_mark, top_level_mark, false),
        react(
            t.cm.clone(),
            Some(t.comments.clone()),
            options.options,
            top_level_mark,
            unresolved_mark,
        ),
        display_name(),
    )
}
test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_add_appropriate_newlines,
    r#"
<Component
  {...props}
  sound="moo" />
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_arrow_functions,
    r#"
var foo = function () {
  return () => <this />;
};

var bar = function () {
  return () => <this.foo />;
};
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_concatenates_adjacent_string_literals,
    r#"
var x =
  <div>
    foo
    {"bar"}
    baz
    <div>
      buz
      bang
    </div>
    qux
    {null}
    quack
  </div>
  "#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_display_name_assignment_expression,
    r#"var Component;
Component = React.createClass({
  render: function render() {
  return null;
  }
});"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_display_name_export_default,
    r#"
export default React.createClass({
  render: function render() {
    return null;
  }
});
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_display_name_if_missing,
    r#"
var Whateva = React.createClass({
  displayName: "Whatever",
  render: function render() {
    return null;
  }
});

var Bar = React.createClass({
  "displayName": "Ba",
  render: function render() {
    return null;
  }
});
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_display_name_object_declaration,
    r#"
exports = {
  Component: React.createClass({
    render: function render() {
      return null;
    }
  })
};"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_display_name_property_assignment,
    r#"
exports.Component = React.createClass({
  render: function render() {
  return null;
  }
});
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_display_name_variable_declaration,
    r#"
var Component = React.createClass({
  render: function render() {
    return null;
  }
});
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_dont_coerce_expression_containers,
    r#"
<Text>
  To get started, edit index.ios.js!!!{"\n"}
  Press Cmd+R to reload
</Text>
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_honor_custom_jsx_comment_if_jsx_pragma_option_set,
    r#"/** @jsx dom */

<Foo></Foo>;

var profile = <div>
  <img src="avatar.png" className="profile" />
  <h3>{[user.firstName, user.lastName].join(" ")}</h3>
</div>;"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_honor_custom_jsx_comment,
    r#"
/** @jsx dom */

<Foo></Foo>;

var profile = <div>
  <img src="avatar.png" className="profile" />
  <h3>{[user.firstName, user.lastName].join(" ")}</h3>
</div>;
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(
        t,
        Options {
            pragma: Some("dom".into()),
            ..Default::default()
        },
        Mark::fresh(Mark::root())
    ),
    react_honor_custom_jsx_pragma_option,
    r#"

<Foo></Foo>;

var profile = <div>
  <img src="avatar.png" className="profile" />
  <h3>{[user.firstName, user.lastName].join(" ")}</h3>
</div>;"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_jsx_with_retainlines_option,
    r#"var div = <div>test</div>;"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_jsx_without_retainlines_option,
    r#"var div = <div>test</div>;"#
);

test!(
    // Optimization is not implemented yet
    ignore,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_optimisation_react_constant_elements,
    r#"
class App extends React.Component {
  render() {
    const navbarHeader = <div className="navbar-header">
      <a className="navbar-brand" href="/">
        <img src="/img/logo/logo-96x36.png" />
      </a>
    </div>;

    return <div>
      <nav className="navbar navbar-default">
        <div className="container">
          {navbarHeader}
        </div>
      </nav>
    </div>;
  }
}
"#
);

#[cfg(feature = "es3")]
test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| (
        tr(t, Default::default(), Mark::fresh(Mark::root())),
        property_literals(),
    ),
    react_should_add_quotes_es3,
    r#"var es3 = <F aaa new const var default foo-bar/>;"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_allow_constructor_as_prop,
    r#"<Component constructor="foo" />;"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_allow_deeper_js_namespacing,
    r#"<Namespace.DeepNamespace.Component />;"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_allow_elements_as_attributes,
    r#"<div attr=<div /> />"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_allow_js_namespacing,
    r#"<Namespace.Component />;"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_allow_nested_fragments,
    r#"
<div>
  <  >
    <>
      <span>Hello</span>
      <span>world</span>
    </>
    <>
      <span>Goodbye</span>
      <span>world</span>
    </>
  </>
</div>
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_allow_no_pragmafrag_if_frag_unused,
    r#"
/** @jsx dom */

<div>no fragment is used</div>
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_allow_pragmafrag_and_frag,
    r#"
/** @jsx dom */
/** @jsxFrag DomFrag */

<></>
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_avoid_wrapping_in_extra_parens_if_not_needed,
    r#"
var x = <div>
  <Component />
</div>;

var x = <div>
  {props.children}
</div>;

var x = <Composite>
  {props.children}
</Composite>;

var x = <Composite>
  <Composite2 />
</Composite>;
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_convert_simple_tags,
    r#"var x = <div></div>;"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_convert_simple_text,
    r#"var x = <div>text</div>;"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_escape_xhtml_jsxattribute,
    r#"
<div id="wôw" />;
<div id="\w" />;
<div id="w &lt; w" />;
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_escape_xhtml_jsxtext_1,
    r"
<div>wow</div>;
<div>wôw</div>;

<div>w & w</div>;
<div>w &amp; w</div>;

<div>w &nbsp; w</div>;
<div>this should parse as unicode: {'\u00a0 '}</div>;

<div>w &lt; w</div>;
"
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_escape_xhtml_jsxtext_2,
    r"
<div>this should not parse as unicode: \u00a0</div>;
"
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_escape_unicode_chars_in_attribute,
    r#"<Bla title="Ú"/>"#
);

test!(
    // FIXME
    ignore,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_escape_xhtml_jsxtext_3,
    r#"
<div>this should parse as nbsp:   </div>;
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_handle_attributed_elements,
    r#"
var HelloMessage = React.createClass({
  render: function() {
    return <div>Hello {this.props.name}</div>;
  }
});

React.render(<HelloMessage name={
  <span>
    Sebastian
  </span>
} />, mountNode);
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_handle_has_own_property_correctly,
    r#"<hasOwnProperty>testing</hasOwnProperty>;"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_have_correct_comma_in_nested_children,
    r#"
var x = <div>
  <div><br /></div>
  <Component>{foo}<br />{bar}</Component>
  <br />
</div>;
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_insert_commas_after_expressions_before_whitespace,
    r#"
var x =
  <div
    attr1={
      "foo" + "bar"
    }
    attr2={
      "foo" + "bar" +

      "baz" + "bug"
    }
    attr3={
      "foo" + "bar" +
      "baz" + "bug"
    }
    attr4="baz">
  </div>
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_not_add_quotes_to_identifier_names,
    r#"var e = <F aaa new const var default foo-bar/>;"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_not_mangle_expressioncontainer_attribute_values,
    r#"<button data-value={"a value\n  with\nnewlines\n   and spaces"}>Button</button>;"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_not_strip_nbsp_even_coupled_with_other_whitespace,
    r#"<div>&nbsp; </div>;"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_not_strip_tags_with_a_single_child_of_nbsp,
    r#"<div>&nbsp;</div>;"#
);

// See https://github.com/swc-project/swc/issues/11392
// HTML entity-encoded whitespace should not be trimmed even in multiline JSX
test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_not_strip_entity_encoded_whitespace_multiline,
    r#"<example>
  foo
  <hr />&#32;
  bar
</example>;"#
);

// Numeric entity &#32; should be preserved as space
test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_preserve_entity_encoded_space,
    r#"<div>&#32;content</div>;"#
);

// Numeric entity &#32; at end of line should be preserved
test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_preserve_trailing_entity_encoded_space,
    r#"<div>content&#32;</div>;"#
);

test!(
    module,
    // Comments are currently stripped out
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_properly_handle_comments_between_props,
    r#"
var x = (
  <div
    /* a multi-line
       comment */
    attr1="foo">
    <span // a double-slash comment
      attr2="bar"
    />
  </div>
);
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_quote_jsx_attributes,
    r#"<button data-value='a value'>Button</button>;"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(
        t,
        Options {
            pragma: Some("h".into()),
            throw_if_namespace: false.into(),
            ..Default::default()
        },
        Mark::fresh(Mark::root())
    ),
    react_should_support_xml_namespaces_if_flag,
    r#"<f:image n:attr />;"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_should_transform_known_hyphenated_tags,
    r#"<font-face />;"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_wraps_props_in_react_spread_for_first_spread_attributes,
    r#"
<Component { ... x } y
={2 } z />
"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_wraps_props_in_react_spread_for_last_spread_attributes,
    r#"<Component y={2} z { ... x } />"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_wraps_props_in_react_spread_for_middle_spread_attributes,
    r#"<Component y={2} { ... x } z />"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    react_attribute_html_entity_quote,
    r#"<Component text="Hello &quot;World&quot;" />"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    use_builtins_assignment,
    r#"var div = <Component {...props} foo="bar" />"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    use_spread_assignment,
    r#"<Component y={2} { ...x } z />"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    issue_229,
    "const a = <>test</>
const b = <div>test</div>"
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| {
        let top_level_mark = Mark::fresh(Mark::root());
        tr(t, Default::default(), top_level_mark)
    },
    issue_351,
    "import React from 'react';

<div />;"
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    issue_481,
    "<span> {foo}</span>;"
);

// https://github.com/swc-project/swc/issues/517
test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| {
        let top_level_mark = Mark::fresh(Mark::root());
        tr(t, Default::default(), top_level_mark)
    },
    issue_517,
    "import React from 'react';
<div style='white-space: pre'>Hello World</div>;"
);

#[test]
fn jsx_text() {
    // Basic cases
    assert_eq!(jsx_text_to_str(" "), *" ");
    assert_eq!(jsx_text_to_str("Hello world"), *"Hello world");

    // Single line with whitespace at edges (should keep as-is)
    assert_eq!(jsx_text_to_str("  Hello world  "), *"  Hello world  ");

    // Empty string
    assert_eq!(jsx_text_to_str(""), *"");

    // Only whitespace (single line)
    assert_eq!(jsx_text_to_str("   "), *"   ");
    assert_eq!(jsx_text_to_str("\t\t"), *"\t\t");

    // Multi-line cases
    assert_eq!(jsx_text_to_str("Hello\nworld"), *"Hello world");
    assert_eq!(jsx_text_to_str("  Hello  \n  world  "), *"  Hello world  ");

    // Multi-line with empty lines
    assert_eq!(jsx_text_to_str("Hello\n\nworld"), *"Hello world");
    assert_eq!(jsx_text_to_str("Hello\n  \n  world"), *"Hello world");

    // Leading/trailing whitespace on multiple lines
    assert_eq!(
        jsx_text_to_str("  Hello  \n  world  \n  test  "),
        *"  Hello world test  "
    );

    // Only whitespace (multi-line) should return empty
    assert_eq!(jsx_text_to_str(" \n "), *"");
    assert_eq!(jsx_text_to_str("\n\n\n"), *"");
    assert_eq!(jsx_text_to_str("  \n  \n  "), *"");

    // Different line endings
    assert_eq!(jsx_text_to_str("Hello\rworld"), *"Hello world");
    assert_eq!(jsx_text_to_str("Hello\r\nworld"), *"Hello world");

    // Mixed whitespace types
    assert_eq!(
        jsx_text_to_str("\t Hello \t\n\t world \t"),
        *"\t Hello world \t"
    );
}

// https://github.com/swc-project/swc/issues/542
test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| tr(t, Default::default(), Mark::fresh(Mark::root())),
    issue_542,
    "let page = <p>Click <em>New melody</em> listen to a randomly generated melody</p>"
);

// regression_2775
test!(
    // Module
    ignore,
    Syntax::Es(EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| {
        let top_level_mark = Mark::fresh(Mark::root());
        let unresolved_mark = Mark::fresh(Mark::root());

        (
            classes(Default::default()),
            jsx(
                t.cm.clone(),
                Some(t.comments.clone()),
                Default::default(),
                top_level_mark,
                unresolved_mark,
            ),
        )
    },
    regression_2775,
    r#"
import React, {Component} from 'react';

export default class RandomComponent extends Component {
constructor(){
super();
}

render() {
return (
  <div className='sui-RandomComponent'>
    <h2>Hi there!</h2>
  </div>
);
}
}

"#
);

test!(
    module,
    Syntax::Es(EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |t| {
        let unresolved_mark = Mark::new();
        let top_level_mark = Mark::new();

        (
            resolver(unresolved_mark, top_level_mark, false),
            jsx(
                t.cm.clone(),
                Some(t.comments.clone()),
                Default::default(),
                top_level_mark,
                unresolved_mark,
            ),
        )
    },
    issue_4956,
    "
    <div title=\"\u{2028}\"/>
    "
);

#[testing::fixture("tests/jsx/fixture/**/input.js")]
fn fixture(input: PathBuf) {
    let mut output = input.with_file_name("output.js");
    if !output.exists() {
        output = input.with_file_name("output.mjs");
    }

    test_fixture(
        Syntax::Es(EsSyntax {
            jsx: true,
            ..Default::default()
        }),
        &|t| {
            let options = parse_options(input.parent().unwrap());
            fixture_tr(t, options)
        },
        &input,
        &output,
        FixtureTestConfig {
            allow_error: true,
            module: Some(true),
            ..Default::default()
        },
    );
}

#[testing::fixture("tests/integration/fixture/**/input.js")]
fn integration(input: PathBuf) {
    let mut output = input.with_file_name("output.js");
    if !output.exists() {
        output = input.with_file_name("output.mjs");
    }

    test_fixture(
        Syntax::Es(EsSyntax {
            jsx: true,
            ..Default::default()
        }),
        &|t| {
            let options = parse_options(input.parent().unwrap());
            integration_tr(t, options)
        },
        &input,
        &output,
        FixtureTestConfig {
            allow_error: true,
            module: Some(true),
            ..Default::default()
        },
    );
}

#[testing::fixture("tests/script/**/input.js")]
fn script(input: PathBuf) {
    let output = input.with_file_name("output.js");

    let options = parse_options(input.parent().unwrap());

    let input = fs::read_to_string(&input).unwrap();

    test_script(&input, &output, options);
}

fn test_script(src: &str, output: &Path, options: Options) {
    Tester::run(|tester| {
        let fm = tester
            .cm
            .new_source_file(FileName::Real("input.js".into()).into(), src.to_string());

        let syntax = Syntax::Es(EsSyntax {
            jsx: true,
            ..Default::default()
        });

        let mut parser = Parser::new(syntax, StringInput::from(&*fm), Some(&tester.comments));

        let script = parser.parse_script().unwrap();

        let top_level_mark = Mark::new();
        let unresolved_mark = Mark::new();

        let script = Program::Script(script).apply((
            resolver(Mark::new(), top_level_mark, false),
            react(
                tester.cm.clone(),
                Some(&tester.comments),
                options,
                top_level_mark,
                unresolved_mark,
            ),
            hygiene::hygiene(),
            fixer(Some(&tester.comments)),
        ));

        let mut buf = Vec::new();

        let mut emitter = Emitter {
            cfg: Config::default()
                .with_ascii_only(true)
                .with_omit_last_semi(true),
            cm: tester.cm.clone(),
            wr: Box::new(swc_ecma_codegen::text_writer::JsWriter::new(
                tester.cm.clone(),
                "\n",
                &mut buf,
                None,
            )),
            comments: Some(&tester.comments),
        };

        // println!("Emitting: {:?}", module);
        emitter.emit_program(&script).unwrap();

        let s = String::from_utf8_lossy(&buf).to_string();
        assert!(NormalizedOutput::new_raw(s).compare_to_file(output).is_ok());

        Ok(())
    })
}
