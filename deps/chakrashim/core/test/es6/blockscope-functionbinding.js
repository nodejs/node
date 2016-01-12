//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function print(x) { WScript.Echo(x); }

print('\nTest 1: Simple case with single block-scoped function\n');
(function ()
{
    // with prior reference
    print(f);
    if (true)
    {
        function f() { }
        print(f);
    }
    print(f);

    // without prior reference
    if (true)
    {
        function g() { }
        print(g);
    }
    print(g);

    // without prior or in-block-scope reference
    if (true)
    {
        function h() { }
    }
    print(h);
})();

print('\nTest 2: Block-scoped function shadows outer let binding, but leaks into function scope\n');
(function ()
{
    print(f);
    if (true)
    {
        let f = 'let f';
        print(f);
        if (true)
        {
            function f() { }
            print(f);
        }
        print(f);
    }
    print(f);
})();


print('\nTest 3: Assignment to function in block behaves like assigning let variable; function still leaks out to function scope; assignment does not affect function scope binding\n');
(function ()
{
    print(f);
    if (true)
    {
        let f = 'let f';
        print(f);
        if (true)
        {
            function f() { }
            f = 'reassigned inner function declared "let" f';
            print(f);
        }
        print(f);
        f = 'reassigned outer let f';
        print(f);
    }
    print(f);
})();


print('\nTest 4: Last executed declaration wins; simple case\n');
(function ()
{
    if (true)
    {
        function f(one) { }
        print(f);
    }
    print(f);
    if (true)
    {
        function f(two) { }
        print(f);
    }
    print(f);
})();


print('\nTest 5: Inner shadows outer and last executed (inner) wins for function scope\n');
(function ()
{
    if (true)
    {
        function f(outer) { }
        print(f);

        if (true)
        {
            function f(inner) { }
            print(f);
        }
        print(f);
    }
    print(f);
})();

print('\nTest 6: Last declaration executed wins; loop and conditionals\n');
(function ()
{
    let x = 0;
    print(f);
    while (x < 3)
    {
        print(f);
        if (x === 0 || x === 2)
        {
            function f(one) { }
            print(f);
        }
        else if (x === 1)
        {
            function f(two) { }
            print(f);
        }
        print(f);

        x += 1;
    }
    print(f);
})();

print('\nTest 7: Make sure function scope binding is initialized even when it is an activation object (imposed by presence of eval)\n');
(function ()
{
    eval('print(f);');

    if (true)
    {
        function f() { }
    }

    eval('print(f);');

    print(f);
})();

print('\nTest 8: Function scope binding does not occur in strict mode\n');
(function ()
{
    for (var i = 0; i < 2; i += 1)
    {
        try
        {
            (function ()
            {
                "use strict";

                if (i == 0)
                    print(f);

                if (true)
                {
                    function f() { }
                    print(f);
                }

                print(f);
            })();
        }
        catch (e)
        {
            print(e);
        }
    }
})();

print('\nTest 9: Overwriting user declared var\n');
(function ()
{
    var f = 'var f';

    print(f);

    if (true)
    {
        function f() { }
        print(f);
    }

    print(f);
})();

print('\nTest 10: inner functions before block scope function should bind to function scoped binding, not outer scope\n');
(function ()
{
    function f()
    {
        print('outer scope f()');
    }

    (function ()
    {
        function g ()
        {
            if (f === undefined)
            {
                print(f);
            }
            else
            {
                f();
            }
        }

        g();

        if (true)
        {
            function f ()
            {
                print('inner block-scope f()');
            }
        }

        g();
    })();
})();

print('\nTest 11: Function declarations shadow with object but also do not assign to with object properties\n');
(function ()
{
    print(f);
    print(g);

    with ({ f: 'with f', g: 'with g' })
    {
        function f() { }
        function h() { }

        print(f);

        if (true)
        {
            function g() { }
            print(g);
        }

        print(g);
        print(h);
    }

    print(f);
    print(g);
    print(h);
})();

print('\nTest 12: Ensure redeclaration errors do not occur with var binding of block scoped functions\n');
(function ()
{
    let f = 'let f';
    print(f);

    {
        function f() { }
        function g() { }

        print(f);
        print(g);
    }

    let g = 'let g';

    print(f);
    print(g);
})();

print('\nTest 13: Eval does not leak let and const bindings\n');
(function ()
{
    var f = 'var f';

    print(f);
    try { print(g); } catch (e) { print(e); }

    eval("var f = 'eval var f'; function g() { } let h = 'eval let h'; const i = 'eval const i'; if (true) { var j = 'eval blockscoped var j'; let k = 'eval blockscoped let k'; }");

    print(f);
    print(g);
    try { print(h); } catch (e) { print(e); }
    try { print(i); } catch (e) { print(e); }
    print(j);
    try { print(k); } catch (e) { print(e); }
})();

print('\nTest 14: Eval leaks vars, and var declarations bind to block scoped bindings for rhs initialization\n');
(function ()
{
    var f = 'var f';

    {
        let f = 'let f';

        eval("var f = 'eval var f'; var g = 'eval var g'; print(f); print(g);");

        print(f);
        print(g);
    }

    print(f);
    print(g);
})();

print('\nTest 15: Eval should have TDZ use before declaration error\n');
(function ()
{
    try
    {
        eval("f(); let x = 'let x'; function f() { print(x); }");
    }
    catch (e)
    {
        print(e);
    }
})();

print('\nTest 16: Eval function declarations create/assign to a var binding and assign to let binding, and create a let binding if not at eval global scope\n');
(function ()
{
    function f(notineval) { }

    if (true)
    {
        function g(notineval) { }
        function h(notineval) { }

        eval("function f(ineval) { }; function g(ineval) { }; if (true) { function h(ineval) { }; print(h); } print(h);");

        print(f); // this should be f(ineval)
        print(g); // this should be g(ineval)
        print(h); // this should be h(notineval)
    }

    print(f); // this should be f(ineval)
    print(g); // this should be g(ineval)
    print(h); // this should be h(ineval)
})();

print('\nTest 17: var initializations should find block scoped lets but still create properties at function scope\n');
(function ()
{
    var f = 'var f not in eval';

    if (true)
    {
        let g = 'let g not in eval';

        eval("var f = 'var f in eval'; if (true) { var g = 'var g in eval'; }");

        print(f); // this should be 'var f in eval'
        print(g); // this should be 'let g in eval'
    }

    print(f); // this should be 'var f in eval'
    print(g); // this should be 'undefined'
})();

print('\nTest 18: function declaration in eval should shadow variables declared outside eval (strict)\n');
(function ()
{
    // Only strict mode has this behavior
    "use strict";
    var f = 'var f';
    if (true)
    {
        let g = 'let g';

        eval("function f() { } function g() { } f(); g(); print(f); print(g);");

        print(g);
    }
    print(f);
    try
    {
        print(g);
    }
    catch (e)
    {
        print(e);
    }
})();

print('\nTest 19: function declaration var binding should be ignored when same named let/const variable is at function scope\n');
(function ()
{
    // Note: declared var's simply get overwritten instead of introducing shadowing

    // static
    (function () {
        let f = 'let f';
        {
            function f() { }
            print(f);
        }
        print(f);
    })();
    (function () {
        const g = 'const g';
        {
            function g() { }
            print(g);
        }
        print(g);
    })();
    (function () {
        var h = 'var h';
        {
            function h() { }
            print(h);
        }
        print(h);
    })();
    (function () {
        {
            function i() { }
            print(i);
        }
        let i = 'let i';
        print(i);
    })();
    (function () {
        {
            function j() { }
            print(j);
        }
        const j = 'const j';
        print(j);
    })();
    (function () {
        {
            function k() { }
            print(k);
        }
        var k = 'var k';
        print(k);
    })();

    // dynamic via eval
    (function () {
        let l = 'let l';
        {
            eval("function l(one) { }; print(l);");
            print(l);
        }
        print(l);

        l = 'outer let l';
        {
            let l = 'inner let l';
            eval("function l(two) { }; print(l);");
            print(l);
        }
        print(l);
    })();
    (function () {
        const m = 'const m';
        {
            try { eval("function m(one) { }; print(m);"); } catch (e) { print(e); }
            print(m);
        }
        print(m);

        {
            const m = 'inner const m';
            try { eval("function m(two) { }; print(m);"); } catch (e) { print(e); }
            print(m);
        }
        print(m);

        {
            let m = 'inner let m';
            try { eval("function m(three) { }; print(m);"); } catch (e) { print(e); }
            print(m);
        }
        print(m);
    })();
    (function () {
        var n = 'var n';
        {
            eval("function n() { }; print(n);");
            print(n);
        }
        print(n);
    })();
    (function () {
        {
            try { eval("function o() { }; print(o);"); } catch (e) { print(e); }
        }
        let o = 'let o';
        print(o);
    })();
    (function () {
        {
            try { eval("function p() { }; print(p);"); } catch (e) { print(e); }
        }
        const p = 'const p';
        print(p);
    })();
    (function () {
        {
            try { eval("function q() { }; print(q);"); } catch (e) { print(e); }
        }
        var q = 'var q';
        print(q);
    })();

    // function var binding should cause scope slot indices to be messed up;
    // see bug 513299.
    function test1() {
        eval('');
        a;
        {
            function b() { }
        }
        const b = 1;
        var a;
    }
    test1();

})();

print('\nTest 20: Function declaration in statement context without {}\n');
(function() {
    if (1)
        function f1() { print('1'); }
    else
        function f1() { print('0'); }

    if (0)
        function f2() { print('1'); }
    else
        function f2() { print('0'); }

    if (1) {
        if (1)
            function f3() { print('1'); }
        else
            function f3() { print('0'); }

        if (0)
            function f4() { print('1'); }
        else
            function f4() { print('0'); }
    }

    while (false)
        function f5() {}

    for (;false;)
        function f6() {}

    for (var p in {a:'a'})
        function f7() {}

    for (var e of [1])
        function f8() {}

    with ({})
        function f9() {}

    f1();
    f2();
    f3();
    f4();

    f7();
    f8();
    f9();
})();

print('\nTest 21: Function declaration in statement context without {}, strict mode\n');
(function() {
    "use strict";

    // The B.3.4 exceptions are not allowed in strict mode

    try {
        eval('if (true)' +
             '    function f() { return "not allowed in strict mode"; }' +
             'else' +
             '    void 0;');
    }
    catch(ex) {
        print('21.1: ' + ex.message);
    }

    try {
        eval('if (true)' +
             '    void 0;' +
             'else' +
             '    function f() { return "not allowed in strict mode"; }');
    }
    catch(ex) {
        print('21.2: ' + ex.message);
    }

    try {
        eval('if (true)' +
             '    function f() { return "not allowed in strict mode"; }' +
             'else' +
             '    function f() { return "not allowed in strict mode"; }');
    }
    catch(ex) {
        print('21.3: ' + ex.message);
    }

    try {
        eval('if (true)' +
             '    function f() { return "not allowed in strict mode"; }');
    }
    catch(ex) {
        print('21.4: ' + ex.message);
    }
})();

print('\nTest 22: Function declaration in statement context without {}, illegal in sloppy mode\n');
(function() {
    // Always illegal syntax regardless of strict mode
    // generator functions are GeneratorFunctionDeclaration, not FunctionDeclaration
    // so are also not allowed by the B.3.4 exception

    try {
        eval('if (true)' +
             '    function* f() { return "never allowed"; }' +
             'else' +
             '    void 0;');
    }
    catch(ex) {
        print('22.1: ' + ex.message);
    }

    try {
        eval('if (true)' +
             '    void 0;' +
             'else' +
             '    function* f() { return "never allowed"; }');
    }
    catch(ex) {
        print('22.2: ' + ex.message);
    }

    try {
        eval('if (true)' +
             '    function* f() { return "never allowed"; }' +
             'else' +
             '    function* f() { return "never allowed"; }');
    }
    catch(ex) {
        print('22.3: ' + ex.message);
    }

    try {
        eval('if (true)' +
             '    function* f() { return "never allowed"; }');
    }
    catch(ex) {
        print('22.4: ' + ex.message);
    }

    // async is ES7 but presumably will also not be FunctionDeclaration in the grammar
    // and so are also not allowed by the B.3.4 exception

    try {
        eval('if (true)' +
             '    async function f() { return "never allowed"; }' +
             'else' +
             '    void 0;');
    }
    catch(ex) {
        print('22.5: ' + ex.message);
    }

    try {
        eval('if (true)' +
             '    void 0;' +
             'else' +
             '    async function f() { return "never allowed"; }');
    }
    catch(ex) {
        print('22.6: ' + ex.message);
    }

    try {
        eval('if (true)' +
             '    async function f() { return "never allowed"; }' +
             'else' +
             '    async function f() { return "never allowed"; }');
    }
    catch(ex) {
        print('22.7: ' + ex.message);
    }

    try {
        eval('if (true)' +
             '    async function f() { return "never allowed"; }' +
             'else' +
             '    async function f() { return "never allowed"; }');
    }
    catch(ex) {
        print('22.8: ' + ex.message);
    }

    try {
        eval('if (true)' +
             '    async function f() { return "never allowed"; }');
    }
    catch(ex) {
        print('22.8: ' + ex.message);
    }
})();

// Leave this test last since it is at global scope and would be awkward to place in the middle of the cleanly contained tests
// above, and also so that it does not accidentally influence the previous tests.
print('\nTest Global: Global scope has the same semantics for block-scoped function declarations\n');

print(glo_f);

if (true)
{
    function glo_f() { }
    print(glo_f);
}

print(glo_f);

function glo_g(globalscope) { }

print(glo_g);

if (true)
{
    function glo_g(blockscope) { }
    print(glo_g);
}

print(glo_g);

print(glo_h);

if (true)
{
    function glo_h(one) { }
    print(glo_h);
}

print(glo_h);

if (true)
{
    function glo_h(two) { }
    print(glo_h);
}

print(glo_h);

print('\nGlobal version of Test 19: function declaration\'s var binding should be ignored when same named let/const variable is at global scope\n');

// Note: declared var's simply get overwritten instead of introducing shadowing

// static
let glo_t19_f = 'let glo_t19_f';
{
    function glo_t19_f() { }
    print(glo_t19_f);
}
print(glo_t19_f);

const glo_t19_g = 'const glo_t19_g';
{
    function glo_t19_g() { }
    print(glo_t19_g);
}
print(glo_t19_g);

var glo_t19_h = 'var glo_t19_h';
{
    function glo_t19_h() { }
    print(glo_t19_h);
}
print(glo_t19_h);

{
    function glo_t19_i() { }
    print(glo_t19_i);
}
let glo_t19_i = 'let glo_t19_i';
print(glo_t19_i);

{
    function glo_t19_j() { }
    print(glo_t19_j);
}
const glo_t19_j = 'const glo_t19_j';
print(glo_t19_j);

{
    function glo_t19_k() { }
    print(glo_t19_k);
}
var glo_t19_k = 'var glo_t19_k';
print(glo_t19_k);


// dynamic via eval
let glo_t19_l = 'let glo_t19_l';
{
    eval("function glo_t19_l(one) { }; print(glo_t19_l);");
    print(glo_t19_l);
}
print(glo_t19_l);
print(this.glo_t19_l);

glo_t19_l = 'outer let glo_t19_l';
{
    let glo_t19_l = 'inner let glo_t19_l';
    eval("function glo_t19_l(two) { }; print(glo_t19_l);");
    print(glo_t19_l);
}
print(glo_t19_l);
print(this.glo_t19_l);

const glo_t19_m = 'const glo_t19_m';
{
    try { eval("function glo_t19_m(one) { }; print(glo_t19_m);"); } catch (e) { print(e); }
    print(glo_t19_m);
}
print(glo_t19_m);
print(this.glo_t19_m);

{
    const glo_t19_m = 'inner const m';
    try { eval("function glo_t19_m(two) { }; print(glo_t19_m);"); } catch (e) { print(e); }
    print(glo_t19_m);
}
print(glo_t19_m);
print(this.glo_t19_m);

{
    let glo_t19_m = 'inner let m';
    try { eval("function glo_t19_m(three) { }; print(glo_t19_m);"); } catch (e) { print(e); }
    print(glo_t19_m);
}
print(glo_t19_m);
print(this.glo_t19_m);

var glo_t19_n = 'var glo_t19_n';
{
    eval("function glo_t19_n() { }; print(glo_t19_n);");
    print(glo_t19_n);
}
print(glo_t19_n);

{
    try { eval("function glo_t19_o() { }; print(glo_t19_o);"); } catch (e) { print(e); }
}
let glo_t19_o = 'let glo_t19_o';
print(glo_t19_o);

{
    try { eval("function glo_t19_p() { }; print(glo_t19_p);"); } catch (e) { print(e); }
}
const glo_t19_p = 'const glo_t19_p';
print(glo_t19_p);

{
    try { eval("function glo_t19_q() { }; print(glo_t19_q);"); } catch (e) { print(e); }
}
var glo_t19_q = 'var glo_t19_q';
print(glo_t19_q);

{
    function xxx(){}
}
try
{ 
    eval('xxx()++');
}
catch(e)
{
    print(e);
}

(function () {
    for (var ngjkfy = 0; ngjkfy < 16; ++ngjkfy) {
        if (ngjkfy % 5 == 2) {
            function y() {
            }
        } else {
            for (qvjjmr = 0; qvjjmr < 4; ++qvjjmr) {
                'u{7cb6}';
            }
        }
    }
    var y = 0;
}());
