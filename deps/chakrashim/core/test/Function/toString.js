//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


var a = 1;
function foo() { var x = 1; }
var m = foo.toString();
WScript.Echo("Test 'toString()' on simple function:")
WScript.Echo(m);
WScript.Echo("Test 'toString()' on builtin function parseFloat:")
WScript.Echo(parseFloat.toString());

var an = function() {
    //anonymous
    a = a + 1;
}
WScript.Echo("Test 'toString()' on anonymous function:")
WScript.Echo(an.toString());

WScript.Echo("Test 'toString()' on an anonymous, unhinted function expression:");
WScript.Echo(function () { });

WScript.Echo("Test 'toString()' on an anonymous, unhinted function expression in parentheses (different behavior in standards mode):");
WScript.Echo((function () { }));

WScript.Echo("Test 'toString()' on parent and nested function:")
function parent() {
    WScript.Echo("in parent");
    var bb = 1;
    function nested() {
        WScript.Echo("in nested");
        bb = 2;
    }
    nested();
    WScript.Echo(nested.toString());

}
parent();
WScript.Echo(parent.toString());

WScript.Echo('Test "somestring".indexOf.toString():')
WScript.Echo("somestring".indexOf.toString());

WScript.Echo('Test "somestring".indexOf:')
WScript.Echo("somestring".indexOf);
