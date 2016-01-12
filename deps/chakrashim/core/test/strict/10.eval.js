//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var x = "global";

(function Test1() {
    write(x);
    eval("var x = 'Test1.eval'; write(x);");
    write(x);
})();

(function Test2() {
    var x = "Test2.local";
    write(x);
    eval("var x = 'Test2.eval'; write(x);");
    write(x);
})();

(function Test3() {
    var str = "'Test3.para'";
    eval("var x = eval; write(x(str));");
})();

(function Test4() {
    var str = "'Test4'";
    eval("function Test4_Inner() { write('inside Test4_inner');} ");
    try {
        Test4_Inner();
    } catch (e) {
        write("Exception : " + e);
    }
})();

eval("'use strict'; function foo(a){eval('');return String(a)}; foo('hello');");
