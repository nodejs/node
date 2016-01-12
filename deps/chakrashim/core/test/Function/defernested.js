//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var echo = WScript.Echo;

var x0 = 0;

echo("Nested in function expression");
(function(){

    var x1 = 1;

    function f1() {
        var x11 = 11;
        echo(x0, x1, x11, typeof f1);
    }
    f1();

    function f2(a2) {
        echo(a2, arguments[0], typeof arguments, typeof f1);
    }
    f2("a2");

})();
echo();

echo("Nested in function expression with arguments");
(function(a0){

    var x1 = 1;

    function f1() {
        echo(x0, x1, a0, typeof f1);
    }
    f1();

})("a0");
echo();

echo("Nested in named function expression, hidden and unhidden");
(function f(a0){

    var x1 = 1;

    function f1() {
        echo(typeof f, x0, x1, a0, typeof f1);
    }
    f1();

})("a0");
(function f(a0, f){

    var x1 = 1;

    function f1() {
        echo(typeof f, x0, x1, a0, typeof f1);
    }
    f1();

})("a0");
echo();

echo("Nested in function expression with eval");
(function(a0){

    eval("x1 = 1; var x2 = 2");

    function f1() {
        echo(x0, x1, x2, a0, typeof f1);
    }
    f1();

    try {
        // Make sure global-eval-scoped functions work right in ES6
        WScript.Echo(eval('let x; function z() { return z; } z();'));
    } catch(e) {}

})("a0");
echo();

echo("Nested in _named_ function expression");
(function f0(){

    var x1 = 1;

    function f1() {
        var x11 = 11;
        echo(x0, x1, x11, typeof f1, typeof f0);
    }
    f1();

    function f2(a2) {
        echo(a2, arguments[0], typeof arguments, typeof f1, typeof f0);
    }
    f2("a2");

})();
echo();

echo("Nested in _named_ function expression with arguments");
(function f0(a0){

    var x1 = 1;

    function f1() {
        echo(x0, x1, a0, typeof f1, typeof f0);
    }
    f1();

})("a0");
echo();

echo("Nested in _named_ function expression with eval");
(function f0(a0, a1){

    eval("x1 = 1; var x3 = 3");
    var x2 = 2;

    function f1() {
        echo(x0, x1, x2, x3, a0, a1, typeof f1, typeof f0);
    }
    f1();

})("a0", "a1");
echo();

echo("Deeply nested");
(function f0(a0, a1){

    eval("x1 = 1");
    var x2 = 2;

    function f1(af1) {

        function f2() {
            echo(x0, x1, x2, a0, a1, af1, typeof f1, typeof f0);
        }
        f2();

    }
    f1("af1");

})("a0", "a1");
echo();

echo("Deeply nested func expr");
(function f0(a0, a1){

    eval("x1 = 1");
    var x2 = 2;

    (function(){
        (function(){
            function f3() {
                echo(x0, x1, x2, a0, a1, typeof f0);
            }
            f3();
        })();
    })();

})("a0", "a1");
echo();

echo("Parent func has arguments");
(function() {
    function foo(a, b) {
        echo(arguments, typeof bar);
        function bar(){}
    }
    foo(1,2);
})();

//-------------------------- eval ---------------------------
var x = "global";
geval = eval;

echo("Child calls eval");
(function(){
    var x = "local";

    function f1(af1) {
        eval("echo(x)");
    }

    f1();
})();

echo("Deeply nested child calls eval");
(function(){
    var x = "local";

    function f1() {
        function f2() {
            (function(){
                function f4() {
                    eval("echo(x)");
                }
                f4();
            })();
        }
        f2();
    }
    f1();
})();

echo("Child calls (eval)");
(function(){
    var x = "local";

    function f1() {
        (eval)("echo(x)");
    }

    f1();
})();

echo("Child calls (,eval)");
(function(){
    var x = "local";

    function f1() {
        (1,eval)("echo(x)");
    }

    f1();
})();

echo("Child calls geval");
(function(){
    var x = "local";

    function f1() {
        geval("echo(x)");
    }
    f1();

})();

echo("Child calls leval");
(function(){
    var x = "local";

    function f1() {
        var leval = eval;
        function f2() {
            leval("echo(x)");
        }
        f2();
    }
    f1();

})();

echo("Parent in strict mode, child eval syntax error");
(function(){
"use strict";

    function f0() {
        function f1() {
            eval("arguments = 42;");
        }
        f1();
    };

    try {
        f0();
    } catch (e) {
        echo(e); // expect syntax error for "arguments = 42"
    }
})();

echo();

//----------------- with -------------------
var a = "global";
var b = "global";
var x = {a:"with"};

echo("func inside with is not deferred");
with (x) {
    var f1 = function() {
        function f2() {
            echo(a, b);
        }
        f2();
    };
    f1();
}

echo("simple with (no outer symbol access)");
(function(){
    function f0() {
        with (x) {
            var f1 = function() {
                echo(a, b);
            };
            f1();
        }
    }
    f0();
})();

echo("simple access from with");
(function(){
    var a = "local";
    var b = "local";
    function f0() {
        with (x) {
            echo(a, b);
        }
    }
    f0();
})();

echo("call func from with");
(function () {
    var obj = {};

    function foo() {
        echo("foo");
    }

    function bar() {
        with (obj) {
            foo();
        }
    }
    bar();
})();

echo("call self from with");
(function() {
    var i = 0;
    function foo() {
        echo("foo", i++);
        if (i > 0) {
            return;
        }

        with (x) {
            foo();
        }
    }
    foo();
})();

echo();

//----------------- try/catch -------------------
echo("parent is catch scope");
(function(){
    try {
        echo(no_such_var);
    } catch(e) {
        // This is inside catch, should not be deferred.
        (0, function(){
            echo(e);
        })();
    }
})();

echo("parent func contains catch scope");
(function(){
    function f0() {

        try {
            echo(no_such_var);
        } catch(e) {
            echo(no_such_var);
        }

        // This can be deferred
        (0, function() {
        })();
    }

    try {
        f0();
    } catch(e) {
        echo(e);
    }
})();

echo("parent func contains catch scope and eval");
(function(){
    function f0() {

        eval("");
        try {
            echo(no_such_var);
        } catch(e) {
            echo(no_such_var);
        }

        // This can be deferred
        (0, function() {
        })();
    }

    try {
        f0();
    } catch(e) {
        echo(e);
    }
})();

echo();
echo("Win8 540999: arguments identifier used as parameter");
(function () {
    function foo() {
        function unwrapArguments(arguments) {
            for (var i = 0; i < arguments.length; i++) {
                echo(arguments[i]);
            }
        }

        function bar() {
            unwrapArguments();
        }

        bar();
    }

    try {
        foo();
    } catch (e) {
        echo(e);
    }
})();

echo();
echo("Win8 649401: Same named parameters + eval");
(function () {
    function foo(x, x, x, x, x, x) {
        function bar() { };
        eval('echo("x:", x)'); // eval
    }
    foo(0);
    foo(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
})();

echo();
echo("Win8 649401: Same named parameters + arguments");
(function () {
    function foo(x, x, x, x, x, x) {
        function bar() { };
        echo("x:", x);

        for (var i = 0; i < arguments.length; i++) {
            echo(arguments[i]);
        }
    }
    foo(0);
    foo(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
})();
