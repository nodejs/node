use swc_ecma_transforms_testing::test;

use super::*;

fn tr() -> impl Pass {
    display_name()
}

test!(
    ::swc_ecma_parser::Syntax::default(),
    |_| tr(),
    assignment_expression,
    r#"
foo = createReactClass({});
bar = React.createClass({});
"#
);

test!(
    ::swc_ecma_parser::Syntax::default(),
    |_| tr(),
    nested,
    r#"
var foo = qux(createReactClass({}));
var bar = qux(React.createClass({}));
"#
);

test!(
    ::swc_ecma_parser::Syntax::default(),
    |_| tr(),
    object_property,
    r#"
({
    foo: createReactClass({})
});
({
    bar: React.createClass({})
});
"#
);

test!(
    ::swc_ecma_parser::Syntax::default(),
    |_| tr(),
    variable_declarator,
    r#"
var foo = createReactClass({});
var bar = React.createClass({});
"#
);

test!(
    ::swc_ecma_parser::Syntax::default(),
    |_| tr(),
    assignment_expression_with_member,
    r#"
foo.x = createReactClass({});
class A extends B { render() { super.x = React.createClass({}) } };
"#
);
