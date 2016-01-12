//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

"use strict";

function write(v) { WScript.Echo(v + ""); }

function exceptToString(ee) {
    if (ee instanceof TypeError) return "TypeError";
    if (ee instanceof ReferenceError) return "ReferenceError";
    if (ee instanceof EvalError) return "EvalError";
    if (ee instanceof SyntaxError) return "SyntaxError";
    return "Unknown Error";
}

var scenarios = [
 "obj.foo",       // obj is configurable false
 "'hello'[0]",
 "'hello'.length",
 "reg.source",
 "reg.global",
 "reg.lastIndex"
];

(function Test1(x) {
    var str = "delete configurable false property";

    var obj = new Object();
    Object.defineProperty(obj, "foo", { configurable: false, value: 20 });

    var reg = /foo/;

    for (var i = 0; i < scenarios.length; ++i) {
        try {
            var r = eval("delete " + scenarios[i]);
            write("Return: " + scenarios[i] + " " + r);
        } catch (e) {
            write("Exception: " + scenarios[i] + " " + exceptToString(e));
        }
    }
})();