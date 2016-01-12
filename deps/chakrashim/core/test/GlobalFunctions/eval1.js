//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// 1. Use eval to initialize an object.
var x = eval("({" + '"' + "3d-cube" + '"' + ":[1]})");
WScript.Echo(x["3d-cube"][0]);
WScript.Echo(x);

// 2. Use eval to assign a global variable
eval("var i = " + '"' + "3d-cube" + '"' + ";");

var str1 = "var x;"
var str2 = "x = 9; g(); x = 8;"

// 3. Use eval to define nested functions. Use nested eval to 
// manipulate local variables. Access a global variable
// with the same name as a local.
eval("function f()"                           + 
     "{"                                      + 
     "    eval(str1);"                        +
     "    function g() { WScript.Echo(x); };" + 
     "    eval(str2);"                         +
     "    return g;"                          + 
     "}; "                                    +
     "WScript.Echo(x[i][0]);" );

// 4. Use eval to call the function defined within the previous eval
// and get back the nested function.
var z = eval("f()");
eval("z()");

// 5. Call function containing eval taking multiple params; pass it too few.
// Test of ArgIn_A handling when params are homed to a heap location.
function foo(a, b, c) {
    eval("WScript.Echo(a);");
    eval("WScript.Echo(b);");
    eval("WScript.Echo(c);");
}

foo("foo.a", "foo.b");

(function () {
    function foo(a) {
        WScript.Echo(foo);
        eval("bar(false)");
        function bar(x) {
            if (x)
                foo(x);
        }
    };
    foo(true);
})();

// 6. Function declarations inside eval should go to the enclosing scope
// (but not to a "with" object).

var O = {xxx:'O.xxx'};
with (O)
{
    eval('function xxx(){}');
}
WScript.Echo(O.xxx);
WScript.Echo(xxx);

(function () { eval("function foobaz() {}") })();
try {
        foobaz();
        WScript.Echo("fail");
} catch(e) {
        WScript.Echo("pass");
}

// 7. Check 'this' inside eval. See WOOB 1127689.
function F(obj)
{
  this.name = "F";
  WScript.Echo("inside eval: this.name = " + obj.eval('this.name'));
}
this.name = "global object";
var f = new F(this);

var test11glob=42;

function test11() {
        var result;
        function test()
        {
                return this.test11glob
        }
        WScript.Echo(test());
        WScript.Echo(eval("test()"));
}

test11();

// Make sure that deeply aliased eval call does the right thing.
var G = this;
G.NSEval = G["eval"];
function alias() {
    var x = 'hello';
    // In compat mode, print hello. In standards mode, print the global x.
    // And in compat mode, run with deferred parsing to make sure the aliasing
    // of "this" persists across parser instances.
    G.NSEval('WScript.Echo(x)');
}
alias();

// bug 1147044
eval("with ({}) (function fibonacci() {})();"); 

// 8. Set up a custom eval that indirectly calls built-in eval, evoke it, and verify the effect.
var q = eval;
var eval = function(s) {
    // Do some extra stuff.
    WScript.Echo("Custom eval:");
    for (var index = 0; index < arguments.length; index++)
    {
        WScript.Echo("arg " + index + " = \'" + arguments[index] + "\'");
    }
    q(s);
}
eval("x[i][0] = 2;");
WScript.Echo(x[i][0]);

// 9. Test that the extra scope chain parameter is hidden.
eval = (function (x, y) { WScript.Echo(y + ''); });
eval('hello');

// 10. Test jitting of a store to a closure-captured block-scoped variable.
function test5810363() {
  (function () {
    if (false) {
      (function () {
        eval('');
      }());
      function func13() {
      }
      while (func13 = func13) {
      }
    }
  }());
}
test5810363();
test5810363();
test5810363();
