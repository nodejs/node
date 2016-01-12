//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function foo() {} 
function bar() {} ;

var fncs = [ "Object", "Function", "Array", "String", "Number", "Boolean", "Date", "RegExp", "foo", "bar"] ;

var f = new foo();
var b = new bar();

var objs = [ "new Object()",
            "f", "b", "foo", "String.fromCharCode", "Array.prototype.concat",            
            "[1,2,3]", "new Array()", "fncs",
            "'hello'", "new String('world')",
            "10", "10.2", "NaN", "new Number(3)", 
            "true", "false", "new Boolean(true)", "new Boolean(false)",
            "new Date()",
            "/a+/"
           ];

function check(str)
{
    try {
        write(str + " : " + eval(str));
    } catch (e) {
        write(" Exception: " + str + ". " + e.message);
    }
}

for (var i=0; i<objs.length ; i++) {
    for (var j=0; j<fncs.length; j++) {
        check(objs[i] + " instanceof " + fncs[j]);
    }
}

var count = 0;

for (var i=0; i<objs.length ; i++) {
    for (var j=0; j<objs.length; j++) {
        check(objs[i] + " instanceof " + objs[j]);
    }
}