//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

// Change arguments inside eval
function ArgInEval1() {
    var old = arguments;
    eval('arguments="test1"');

    write("old.length       : " + old.length);
    write("arguments.length : " + arguments.length);
    write("Compare the two  : " + (old === arguments));
}

ArgInEval1(10);

function ArgInEval2() {
    var old = arguments;
    eval('write(arguments); var arguments = "test2"; write(arguments);');

    write("old.length       : " + old.length);
    write("arguments.length : " + arguments.length);
    write("Compare the two  : " + (old === arguments));
}

ArgInEval2(10);

function sameNameFormal(x, x) {
    var x = 999;
    write(arguments[0]);
    write(arguments[1]);
}

sameNameFormal(1, 2);

function bug1127540(a, b, c, d) {
    eval("function foo(){return arguments.caller;}");
    write("bug1127540: " + (foo() === arguments));
}

bug1127540(1, 2, 3, 4)


function DelArgs(a) {
    write(arguments[0]);
    delete arguments[0];
    write(arguments[0]);
    arguments[0] = 2;
    write(arguments[0]);
    delete arguments[0];
    write(arguments[0]);
}

DelArgs(1);

// Parent function doesn't use arguments. Modify function.arguments in child
function Func1(a, b, c) {
    a = "assignToFormal";
    Func1.arguments[1] = "funcArgs[1]";
    Func1_Helper();
    write("Arguments : " + Func1.arguments[0] + " " + Func1.arguments[1] + " " + Func1.arguments[2]);
    write("Formals   : " + a + " " + b + " " + c);
}

function Func1_Helper() {
    Func1.arguments[2] = "nested[2]";
    write("In Helper : " + Func1.arguments[0] + " " + Func1.arguments[1] + " " + Func1.arguments[2]);
}

Func1("arg1", "arg2", "arg3");

// Parent function uses arguments. Modify function.arguments in child
function Func2(a, b, c) {
    a = "assignToFormal";
    Func2.arguments[1] = "funcArgs[1]";
    arguments[2] = "arguments[2]";
    Func2_Helper();
    write("Arguments : " + Func2.arguments[0] + " " + Func2.arguments[1] + " " + Func2.arguments[2]);
    write("Formals   : " + a + " " + b + " " + c);
}

function Func2_Helper() {
    Func2.arguments[2] = "nested[2]";
    write("In Helper : " + Func2.arguments[0] + " " + Func2.arguments[1] + " " + Func2.arguments[2]);
}

Func2("arg1", "arg2", "arg3");

function argEscapes(x)
{
    return arguments;
}

do
{
    // Put this in a loop so we'll try to apply the scope-caching optimization.
    var argSave = argEscapes('First call');
    WScript.Echo(argSave[0]);
    var argSave2 = argEscapes('Second call');
    WScript.Echo(argSave[0]);
    WScript.Echo(argSave2[0]);
}
while(0);

function argEscapesViaEval(x)
{
    return eval('arguments');
}

do
{
    // Put this in a loop so we'll try to apply the scope-caching optimization.
    var argSave = argEscapesViaEval('First call');
    WScript.Echo(argSave[0]);
    var argSave2 = argEscapesViaEval('Second call');
    WScript.Echo(argSave[0]);
    WScript.Echo(argSave2[0]);
}
while(0);
