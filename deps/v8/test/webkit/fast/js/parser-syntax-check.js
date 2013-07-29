// Copyright 2013 the V8 project authors. All rights reserved.
// Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

description(
"This test checks that the following expressions or statements are valid ECMASCRIPT code or should throw parse error"
);

function runTest(_a, errorType)
{
    var success;
    if (typeof _a != "string")
        testFailed("runTest expects string argument: " + _a);
    try {
        eval(_a);
        success = true;
    } catch (e) {
        success = !(e instanceof SyntaxError);
    }
    if ((!!errorType) == !success) {
        if (errorType)
            testPassed('Invalid: "' + _a + '"');
        else
            testPassed('Valid:   "' + _a + '"');
    } else {
        if (errorType)
            testFailed('Invalid: "' + _a + '" should throw ' + errorType.name);
        else
            testFailed('Valid:   "' + _a + '" should NOT throw ');
    }
}

function valid(_a)
{
    // Test both the grammar and the syntax checker
    runTest(_a, false);
    runTest("function f() { " + _a + " }", false);
}

function invalid(_a, _type)
{
    _type = _type || SyntaxError;
    // Test both the grammar and the syntax checker
    runTest(_a, true);
    runTest("function f() { " + _a + " }", true);
}

// known issue:
//   some statements requires statement as argument, and
//   it seems the End-Of-File terminator is converted to semicolon
//      "a:[EOF]" is not parse error, while "{ a: }" is parse error
//      "if (a)[EOF]" is not parse error, while "{ if (a) }" is parse error
// known issues of bison parser:
//   accepts: 'function f() { return 6 + }' (only inside a function declaration)
//   some comma expressions: see reparsing-semicolon-insertion.js

debug  ("Unary operators and member access");

valid  ("");
invalid("(a");
invalid("a[5");
invalid("a[5 + 6");
invalid("a.");
invalid("()");
invalid("a.'l'");
valid  ("a: +~!new a");
invalid("new -a");
valid  ("new (-1)")
valid  ("a: b: c: new f(x++)++")
valid  ("(a)++");
valid  ("(1--).x");
invalid("a-- ++");
invalid("(a:) --b");
valid  ("++ -- ++ a");
valid  ("++ new new a ++");
valid  ("delete void 0");
invalid("delete the void");
invalid("(a++");
valid  ("++a--");
valid  ("++((a))--");
valid  ("(a.x++)++");
invalid("1: null");
invalid("+-!~");
invalid("+-!~((");
invalid("a)");
invalid("a]");
invalid(".l");
invalid("1.l");
valid  ("1 .l");

debug  ("Binary and conditional operators");

valid  ("a + + typeof this");
invalid("a + * b");
invalid("a ? b");
invalid("a ? b :");
invalid("%a");
invalid("a-");
valid  ("a = b ? b = c : d = e");
valid  ("s: a[1].l ? b.l['s'] ? c++ : d : true");
valid  ("a ? b + 1 ? c + 3 * d.l : d[5][6] : e");
valid  ("a in b instanceof delete -c");
invalid("a in instanceof b.l");
valid  ("- - true % 5");
invalid("- false = 3");
valid  ("a: b: c: (1 + null) = 3");
valid  ("a[2] = b.l += c /= 4 * 7 ^ !6");
invalid("a + typeof b += c in d");
invalid("typeof a &= typeof b");
valid  ("a: ((typeof (a))) >>>= a || b.l && c");
valid  ("a: b: c[a /= f[a %= b]].l[c[x] = 7] -= a ? b <<= f : g");
valid  ("-void+x['y'].l == x.l != 5 - f[7]");

debug  ("Function calls (and new with arguments)");

valid  ("a()()()");
valid  ("s: l: a[2](4 == 6, 5 = 6)(f[4], 6)");
valid  ("s: eval(a.apply(), b.call(c[5] - f[7]))");
invalid("a(");
invalid("a(5");
invalid("a(5,");
invalid("a(5,)");
invalid("a(5,6");
valid  ("a(b[7], c <d> e.l, new a() > b)");
invalid("a(b[5)");
invalid("a(b.)");
valid  ("~new new a(1)(i++)(c[l])");
invalid("a(*a)");
valid  ("((((a))((b)()).l))()");
valid  ("(a)[b + (c) / (d())].l--");
valid  ("new (5)");
invalid("new a(5");
valid  ("new (f + 5)(6, (g)() - 'l'() - true(false))");
invalid("a(.length)");

debug  ("function declaration and expression");

valid  ("function f() {}");
valid  ("function f(a,b) {}");
invalid("function () {}");
invalid("function f(a b) {}");
invalid("function f(a,) {}");
invalid("function f(a,");
invalid("function f(a, 1) {}");
valid  ("function g(arguments, eval) {}");
valid  ("function f() {} + function g() {}");
invalid("(function a{})");
invalid("(function this(){})");
valid  ("(delete new function f(){} + function(a,b){}(5)(6))");
valid  ("6 - function (m) { function g() {} }");
invalid("function l() {");
invalid("function l++(){}");

debug  ("Array and object literal, comma operator");

// Note these are tested elsewhere, no need to repeat those tests here
valid  ("[] in [5,6] * [,5,] / [,,5,,] || [a,] && new [,b] % [,,]");
invalid("[5,");
invalid("[,");
invalid("(a,)");
valid  ("1 + {get get(){}, set set(a){}, get1:4, set1:get-set, }");
invalid("1 + {a");
invalid("1 + {a:");
invalid("1 + {get l(");
invalid(",a");
valid  ("(4,(5,a(3,4))),f[4,a-6]");
invalid("(,f)");
invalid("a,,b");
invalid("a ? b, c : d");

debug  ("simple statements");

valid  ("{ }");
invalid("{ { }");
valid  ("{ ; ; ; }");
valid  ("a: { ; }");
invalid("{ a: }");
valid  ("{} f; { 6 + f() }");
valid  ("{ a[5],6; {} ++b-new (-5)() } c().l++");
valid  ("{ l1: l2: l3: { this } a = 32 ; { i++ ; { { { } } ++i } } }");
valid  ("if (a) ;");
invalid("{ if (a) }");
invalid("if a {}");
invalid("if (a");
invalid("if (a { }");
valid  ("x: s: if (a) ; else b");
invalid("else {}");
valid  ("if (a) if (b) y; else {} else ;");
invalid("if (a) {} else x; else");
invalid("if (a) { else }");
valid  ("if (a.l + new b()) 4 + 5 - f()");
valid  ("if (a) with (x) ; else with (y) ;");
invalid("with a.b { }");
valid  ("while (a() - new b) ;");
invalid("while a {}");
valid  ("do ; while(0) i++"); // Is this REALLY valid? (Firefox also accepts this)
valid  ("do if (a) x; else y; while(z)");
invalid("do g; while 4");
invalid("do g; while ((4)");
valid  ("{ { do do do ; while(0) while(0) while(0) } }");
valid  ("do while (0) if (a) {} else y; while(0)");
valid  ("if (a) while (b) if (c) with(d) {} else e; else f");
invalid("break ; break your_limits ; continue ; continue living ; debugger");
invalid("debugger X");
invalid("break 0.2");
invalid("continue a++");
invalid("continue (my_friend)");
valid  ("while (1) break");
valid  ("do if (a) with (b) continue; else debugger; while (false)");
invalid("do if (a) while (false) else debugger");
invalid("while if (a) ;");
valid  ("if (a) function f() {} else function g() {}");
valid  ("if (a()) while(0) function f() {} else function g() {}");
invalid("if (a()) function f() { else function g() }");
invalid("if (a) if (b) ; else function f {}");
invalid("if (a) if (b) ; else function (){}");
valid  ("throw a");
valid  ("throw a + b in void c");
invalid("throw");

debug  ("var and const statements");

valid  ("var a, b = null");
valid  ("const a = 5, b, c");
invalid("var");
invalid("var = 7");
invalid("var c (6)");
valid  ("if (a) var a,b; else const b, c");
invalid("var 5 = 6");
valid  ("while (0) var a, b, c=6, d, e, f=5*6, g=f*h, h");
invalid("var a = if (b) { c }");
invalid("var a = var b");
valid  ("const a = b += c, a, a, a = (b - f())");
invalid("var a %= b | 5");
invalid("var (a) = 5");
invalid("var a = (4, b = 6");
invalid("const 'l' = 3");
invalid("var var = 3");
valid  ("var varr = 3 in 1");
valid  ("const a, a, a = void 7 - typeof 8, a = 8");
valid  ("const x_x = 6 /= 7 ? e : f");
invalid("var a = ?");
invalid("const a = *7");
invalid("var a = :)");
valid  ("var a = a in b in c instanceof d");
invalid("var a = b ? c, b");
invalid("const a = b : c");

debug  ("for statement");

valid  ("for ( ; ; ) { break }");
valid  ("for ( a ; ; ) { break }");
valid  ("for ( ; a ; ) { break }");
valid  ("for ( ; ; a ) { break }");
valid  ("for ( a ; a ; ) break");
valid  ("for ( a ; ; a ) break");
valid  ("for ( ; a ; a ) break");
invalid("for () { }");
invalid("for ( a ) { }");
invalid("for ( ; ) ;");
invalid("for a ; b ; c { }");
invalid("for (a ; { }");
invalid("for ( a ; ) ;");
invalid("for ( ; a ) break");
valid  ("for (var a, b ; ; ) { break } ");
valid  ("for (var a = b, b = a ; ; ) break");
valid  ("for (var a = b, c, d, b = a ; x in b ; ) { break }");
valid  ("for (var a = b, c, d ; ; 1 in a()) break");
invalid("for ( ; var a ; ) break");
invalid("for (const a; ; ) break");
invalid("for ( %a ; ; ) { }");
valid  ("for (a in b) break");
valid  ("for (a() in b) break");
valid  ("for (a().l[4] in b) break");
valid  ("for (new a in b in c in d) break");
valid  ("for (new new new a in b) break");
invalid("for (delete new a() in b) break");
invalid("for (a * a in b) break");
valid  ("for ((a * a) in b) break");
invalid("for (a++ in b) break");
valid  ("for ((a++) in b) break");
invalid("for (++a in b) break");
valid  ("for ((++a) in b) break");
invalid("for (a, b in c) break");
invalid("for (a,b in c ;;) break");
valid  ("for (a,(b in c) ;;) break");
valid  ("for ((a, b) in c) break");
invalid("for (a ? b : c in c) break");
valid  ("for ((a ? b : c) in c) break");
valid  ("for (var a in b in c) break");
valid  ("for (var a = 5 += 6 in b) break");
invalid("for (var a += 5 in b) break");
invalid("for (var a = in b) break");
invalid("for (var a, b in b) break");
invalid("for (var a = -6, b in b) break");
invalid("for (var a, b = 8 in b) break");
valid  ("for (var a = (b in c) in d) break");
invalid("for (var a = (b in c in d) break");
invalid("for (var (a) in b) { }");
valid  ("for (var a = 7, b = c < d >= d ; f()[6]++ ; --i()[1]++ ) {}");

debug  ("try statement");

invalid("try { break } catch(e) {}");
valid  ("try {} finally { c++ }");
valid  ("try { with (x) { } } catch(e) {} finally { if (a) ; }");
invalid("try {}");
invalid("catch(e) {}");
invalid("finally {}");
invalid("try a; catch(e) {}");
invalid("try {} catch(e) a()");
invalid("try {} finally a()");
invalid("try {} catch(e)");
invalid("try {} finally");
invalid("try {} finally {} catch(e) {}");
invalid("try {} catch (...) {}");
invalid("try {} catch {}");
valid  ("if (a) try {} finally {} else b;");
valid  ("if (--a()) do with(1) try {} catch(ke) { f() ; g() } while (a in b) else {}");
invalid("if (a) try {} else b; catch (e) { }");
invalid("try { finally {}");

debug  ("switch statement");

valid  ("switch (a) {}");
invalid("switch () {}");
invalid("case 5:");
invalid("default:");
invalid("switch (a) b;");
invalid("switch (a) case 3: b;");
valid  ("switch (f()) { case 5 * f(): default: case '6' - 9: ++i }");
invalid("switch (true) { default: case 6: default: }");
invalid("switch (l) { f(); }");
invalid("switch (l) { case 1: ; a: case 5: }");
valid  ("switch (g() - h[5].l) { case 1 + 6: a: b: c: ++f }");
invalid("switch (g) { case 1: a: }");
invalid("switch (g) { case 1: a: default: }");
invalid("switch g { case 1: l() }");
invalid("switch (g) { case 1:");
valid  ("switch (l) { case a = b ? c : d : }");
valid  ("switch (sw) { case a ? b - 7[1] ? [c,,] : d = 6 : { } : }");
invalid("switch (l) { case b ? c : }");
valid  ("switch (l) { case 1: a: with(g) switch (g) { case 2: default: } default: }");
invalid("switch (4 - ) { }");
invalid("switch (l) { default case: 5; }");

invalid("L: L: ;");
invalid("L: L1: L: ;");
invalid("L: L1: L2: L3: L4: L: ;");

invalid("for(var a,b 'this shouldn\'t be allowed' false ; ) ;");
invalid("for(var a,b '");

valid("function __proto__(){}")
valid("(function __proto__(){})")
valid("'use strict'; function __proto__(){}")
valid("'use strict'; (function __proto__(){})")

valid("if (0) $foo; ")
valid("if (0) _foo; ")
valid("if (0) foo$; ")
valid("if (0) foo_; ")
valid("if (0) obj.$foo; ")
valid("if (0) obj._foo; ")
valid("if (0) obj.foo$; ")
valid("if (0) obj.foo_; ")
valid("if (0) obj.foo\\u03bb; ")
valid("if (0) new a(b+c).d = 5");
valid("if (0) new a(b+c) = 5");
valid("([1 || 1].a = 1)");
valid("({a: 1 || 1}.a = 1)");

invalid("var a.b = c");
invalid("var a.b;");

try { eval("a.b.c = {};"); } catch(e1) { e=e1; shouldBe("e.line", "1") }
foo = 'FAIL';
bar = 'PASS';
try {
     eval("foo = 'PASS'; a.b.c = {}; bar  = 'FAIL';");
} catch(e) {
     shouldBe("foo", "'PASS'");
     shouldBe("bar", "'PASS'");
}
