//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function exceptToString(ee) {
    if (ee instanceof TypeError) return "TypeError";
    if (ee instanceof ReferenceError) return "ReferenceError";
    if (ee instanceof EvalError) return "EvalError";
    if (ee instanceof SyntaxError) return "SyntaxError";
    return "Unknown Error";
}

var gVarTest1 = 1;

(function Test1() {
    var str = "delete global variable";
    try {
        eval("var r = delete gVarTest1;");
        write("r : " + r);
    } catch (e) {
        write("Exception: " + str + " " + exceptToString(e));
        return;
    }
    write("Return: " + str);
}) ();

function gHelperFunction2() {}
(function Test2() {
    var str = "delete global function";
    try {
        eval("var r = delete gHelperFunction2;");
        write("r : " + r);
    } catch (e) {
        write("Exception: " + str + " " + exceptToString(e));
        return;
    }
    write("Return: " + str);
}) ();

(function Test3() {
    var str = "delete local variable";
    var local = 3;
    try {
        eval("var r = delete local;");
        write("r : " + r);
    } catch (e) {
        write("Exception: " + str + " " + exceptToString(e));
        return;
    }
    write("Return: " + str);
}) ();

(function Test4() {
    var str = "delete local function";
    
    var nestedTest4 = function nestedTest4() {} ;

    try {
        eval("var r = delete nestedTest4;");
        write("r : " + r);
    } catch (e) {
        write("Exception: " + str + " " + exceptToString(e));
        return;
    }
    write("Return: " + str);
}) ();

(function Test5(x) {
    var str = "delete parameter";

    try {
        eval("var r = delete x;");
    } catch (e) {
        write("Exception: " + str + " " + exceptToString(e));
        return;
    }
    write("Return: " + str);
}) ();