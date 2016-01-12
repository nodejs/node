//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Test behavior of cached scopes.
// For instance, make sure we do the right thing in same-named-formals cases.

// 1. Case where function with a cached scope has "arguments".
function f1(x, x) {
    WScript.Echo(x);
    eval('arguments[0] = 0');
    eval('arguments[1] = 1');
    WScript.Echo(x);
}
// Call f from inside a loop to force the scope to be cached.
for (var i = 0; i < 1; i++) {
    f1('arg 0', 'arg 1');
}

// 2. Case with no "arguments" in the cached-scope function itself.
function f2(x, x) {
    WScript.Echo(x);
    function g() {
        eval('x = "arg 1"');
    }
    g();
    WScript.Echo(x);
}
// Call f from inside a loop to force the scope to be cached.
for (var i = 0; i < 1; i++) {
    f2('arg 0');
}

// 3. Case where nested function is cached and then undeferred (execute with /forcedeferparse).
function f3(i) {
    function inner() {
        WScript.Echo('inner');
    }
    if (i % 2 != 0) {
        return eval('inner()');
    }
    f3(i + 1);
}

for (i = 0; i < 2; i++)
    f3(i);

try
{
    new Function("let ifviki, eval = (z =  /x/g );L:switch(z) {  case eval(\"z\"): return 503599627370497;break;  }")();
}
catch(e)
{
    WScript.Echo(e.message);
}
