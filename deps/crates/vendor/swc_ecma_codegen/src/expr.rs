/// Copied from [ratel][]
///
/// [ratel]:https://github.com/ratel-rust/ratel-core
#[cfg(test)]
mod tests {
    use crate::tests::assert_min;

    #[test]
    fn values() {
        assert_min("null", "null");
        assert_min("undefined", "undefined");
        assert_min("true", "true");
        assert_min("false", "false");
        assert_min("42", "42");
        assert_min("3.14", "3.14");
        assert_min(r#" 'foobar' "#, r#""foobar""#);
    }

    #[test]
    fn bin_expr() {
        assert_min("1+2+3+4+5", "1+2+3+4+5");
    }

    #[test]
    fn template_expression() {
        assert_min("``", "``");
        assert_min("foo``", "foo``");
        assert_min("`foobar`", "`foobar`");
        assert_min("foo`bar`", "foo`bar`");
        assert_min("`foo${ 10 }bar${ 20 }baz`", "`foo${10}bar${20}baz`");
        assert_min("foo`bar${ 10 }baz`", "foo`bar${10}baz`");
        assert_min("foo`${ 10 }`", "foo`${10}`");
    }

    #[test]
    fn sequence_expression() {
        assert_min("foo, bar, baz;", "foo,bar,baz");
        assert_min("1, 2, 3;", "1,2,3");
        assert_min("1,2,3+4;", "1,2,3+4");
        assert_min("1+2,3,4;", "1+2,3,4");
        assert_min("1+(2,3,4);", "1+(2,3,4)");
        assert_min("(1,2,3)+4;", "(1,2,3)+4");
    }

    #[test]
    fn binary_expression() {
        assert_min("a = 10", "a=10");
        assert_min("a == 10", "a==10");
        assert_min("a === 10", "a===10");
        assert_min("a != 10", "a!=10");
        assert_min("a !== 10", "a!==10");
        assert_min("a += 10", "a+=10");
        assert_min("a -= 10", "a-=10");
        assert_min("a <<= 10", "a<<=10");
        assert_min("a >>= 10", "a>>=10");
        assert_min("a >>>= 10", "a>>>=10");
        assert_min("2 + 2", "2+2");
        assert_min("2 - 2", "2-2");
        assert_min("2 * 2", "2*2");
        assert_min("2 / 2", "2/2");
        assert_min("2 % 2", "2%2");
        assert_min("2 ** 2", "2**2");
        assert_min("2 << 2", "2<<2");
        assert_min("2 >> 2", "2>>2");
        assert_min("2 >>> 2", "2>>>2");
        assert_min("foo in bar", "foo in bar");
        assert_min("foo instanceof Foo", "foo instanceof Foo");

        assert_min("foo() in b", "foo()in b");
        assert_min("typeof foo() in b", "typeof foo()in b");
        assert_min("`` in b", "``in b");
        assert_min("a?.foo() in b", "a?.foo()in b");
        assert_min("++a[1] in b", "++a[1]in b");
        assert_min("a++ in foo", "a++in foo");
        assert_min("``+`` in b", "``+``in b");
        assert_min("new Foo(a) in b", "new Foo(a)in b");

        assert_min("new Foo() in b", "new Foo in b");
        assert_min("++a in b", "++a in b");
    }

    #[test]
    fn prefix_expression() {
        assert_min("+foo", "+foo");
        assert_min("-foo", "-foo");
        assert_min("!foo", "!foo");
        assert_min("~foo", "~foo");
        assert_min("++foo", "++foo");
        assert_min("--foo", "--foo");
        assert_min("new foo", "new foo");
        assert_min("new foo()", "new foo");
        assert_min("new foo().bar()", "new foo().bar()");
        assert_min("void foo", "void foo");
        assert_min("typeof foo", "typeof foo");
    }

    #[test]
    fn postfix_expression() {
        assert_min("foo++", "foo++");
        assert_min("foo--", "foo--");
    }

    #[test]
    fn conditional_expression() {
        assert_min("true ? foo : bar", "true?foo:bar")
    }

    #[test]
    fn function_expression() {
        assert_min("(function () {})", "(function(){})");
        assert_min("(function foo() {})", "(function foo(){})");
    }

    #[test]
    #[ignore]
    fn class_expression() {
        assert_min("(class {})", "(class{})");
        assert_min("(class Foo {})", "(class Foo{})");
        assert_min("(class extends Foo {})", "(class extends Foo{})");
        assert_min("(class Foo extends Bar {})", "(class Foo extends Bar{})");
    }

    #[test]
    fn call_expression() {
        assert_min("foobar();", "foobar()");
        assert_min("foobar(1, 2, 3);", "foobar(1,2,3)");
    }

    #[test]
    fn member_expression() {
        assert_min("foo.bar", "foo.bar");
        assert_min("this.bar", "this.bar");
        assert_min("10..fooz", "10..fooz");
        assert_min("foo[10]", "foo[10]");
        assert_min(r#"foo["bar"]"#, r#"foo["bar"]"#);
    }

    #[test]
    fn array_expression() {
        assert_min("[]", "[]");
        assert_min("[foo]", "[foo]");
        assert_min("[foo,bar]", "[foo,bar]");
        assert_min("[foo,bar,baz,]", "[foo,bar,baz]");
    }

    #[test]
    fn array_spread() {
        assert_min("[...foo,...bar]", "[...foo,...bar]");
    }

    #[test]
    fn sparse_array_expression() {
        assert_min("[]", "[]");
        assert_min("[,]", "[,]");
        assert_min("[,1]", "[,1]");
        assert_min("[,,];", "[,,]");
        assert_min("[1,,];", "[1,,]");
        assert_min("[,,1];", "[,,1]");
    }

    // #[test]
    // fn sparse_array_expression_pretty() {
    //     assert_pretty("[]", "[];");
    //     assert_pretty("[,]", "[, ];");
    //     assert_pretty("[1,]", "[1, ];");
    //     assert_pretty("[,1]", "[, 1];");
    //     assert_pretty("[,,];", "[, , ];");
    //     assert_pretty("[1,,];", "[1, , ];");
    //     assert_pretty("[,,1];", "[, , 1];");
    // }

    #[test]
    fn object_expression() {
        assert_min("({});", "({})");
        assert_min("({ foo });", "({foo})");
        assert_min("({ foo: 10 });", "({foo:10})");
        assert_min("({ foo, bar });", "({foo,bar})");
        assert_min("({ foo: 10, bar: 20 });", "({foo:10,bar:20})");
        assert_min("({ foo: 10, bar() {} });", "({foo:10,bar(){}})");
        assert_min("({ foo(bar, baz) {} });", "({foo(bar,baz){}})");
        // let expected = "({\n    foo: true,\n    bar: false\n});";
        // assert_pretty("({ foo: true, bar: false })", expected);
    }

    #[test]
    fn binding_power() {
        assert_min("1 + 2 * 3;", "1+2*3");
        assert_min("1 + 2 * 3;", "1+2*3");
        assert_min("(1 + 2) * 3;", "(1+2)*3");
        assert_min(
            "(denominator / divider * 100).toFixed(2);",
            "(denominator/divider*100).toFixed(2)",
        );
        assert_min("(1 + 1)[0];", "(1+1)[0]");
        assert_min("2 * 2 / 2;", "2*2/2");
        assert_min("2 * (2 / 2);", "2*(2/2)");
        assert_min("2 * 2 / 2;", "2*2/2");
    }

    #[test]
    fn regression_increments() {
        assert_min("x++ + ++y", "x+++ ++y");
        assert_min("x++ - ++y", "x++-++y");
    }

    #[test]
    fn bigint_property_key() {
        assert_min("({ 1n: 2 });", "({1n:2})");
        assert_min("(class C { 1n = 1 });", "(class C{1n=1})");
        assert_min("(class C { 1n () { } });", "(class C{1n(){}})");
    }

    #[test]
    fn html_comment() {
        assert_min("a < !--b && c-- > d;", "a< !--b&&c-- >d");
    }
}
