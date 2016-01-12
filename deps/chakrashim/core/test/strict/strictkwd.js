//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function Execute(s) {
    try {
        eval(s);
        WScript.Echo("No error");
    } catch (e) {
        WScript.Echo(e);
    }
}

Execute("(function() { function foo1() { 'use strict'; var bar1 = function yield(x) { }; }; foo1(); })();");
Execute("(function() { function foo2() { var bar2 = function yield(x) { }; }; foo2(); })();");
Execute("(function() { function foo3() { function f(yield) { 'use strict'; }; }; foo3(); })();");
Execute("(function() { function foo3() { function f(yield) { }; }; foo3(); })();");
