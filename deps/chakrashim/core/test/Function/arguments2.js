//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var errorCount = 0;
var r;

function verify(scen, act, exp) {
    write(scen + " : " + act);
    if (act !== exp) {
        write("FAILED : " + scen + " exp: " + exp + " act : " + act);
        errorCount++;
    } else {
        write("PASSED : " + scen + " exp: " + exp + " act : " + act);
    }
}

// --------------------------------------------------------------------------------------------------
function Test1(a, b) {
    write(arguments.hasOwnProperty("0"));
    write(arguments.hasOwnProperty("length"));
    return arguments[0] + arguments[1];
}

r = Test1(10, 20);
verify("Test1", r, 30);

// --------------------------------------------------------------------------------------------------
function Test2(a, b) {
    a = 100;
    verify("Test2 arguments[0]", arguments[0], 100);

    b = 200;
    verify("Test2 arguments[1]", arguments[1], 200);

    arguments[0] = 300;
    verify("Test2 a", a, 300);

    arguments[1] = 400;
    verify("Test2 b", b, 400);

    return a + b + arguments[0] + arguments[1];
}

r = Test2(10, 20);
verify("Test2", r, 1400);

// --------------------------------------------------------------------------------------------------
function Test3(a, b) {
    var arguments = new Object();

    if (arguments[0] == a)
        verify("Test3 a", a);

    if (arguments[1] == b)
        verify("Test3 b", b);

    arguments[0] = a;
    arguments[1] = b;

    a = a + 1;
    b = b + 1;

    if (arguments[0] == a)
        verify("Test3 a1", a);

    if (arguments[1] == b)
        verify("Test3 b1", b);

    return 0;
}

r = Test3(10, 20);
verify("Test3", r, 0);

// --------------------------------------------------------------------------------------------------
function Test4(a, b, arguments) {
    if (arguments[0] == a)
        verify("Test4 a", a);

    if (arguments[1] == b)
        verify("Test4 b", b);

    arguments[0] = a;
    arguments[1] = b;

    a = a + 1;
    b = b + 1;

    if (arguments[0] == a)
        verify("Test4 a1", a);

    if (arguments[1] == b)
        verify("Test4 b1", b);

    return 0;
}

r = Test4(10, 20, new Object());
verify("Test4", r, 0);

// --------------------------------------------------------------------------------------------------
function Test5(a, b) {
    var count = 0;

    arguments[0] = 100;
    arguments[1] = 200;

    for (var i in arguments) {
        count++;
    }

    return count;
}

r = Test5(10);
verify("Test5 1", r, 2);

r = Test5(10, 20);
verify("Test5 2", r, 2);

r = Test5(10, 20, 30);
verify("Test5 3", r, 3);

// --------------------------------------------------------------------------------------------------
function Test6(a, b) {
    var count = 0;

    for (var i in arguments) {
        count = count + 1;
    }

    return count;
}

r = Test6(10);
verify("Test6 1", r, 1);

r = Test6(10, 20);
verify("Test6 2", r, 2);

r = Test6(10, 20, 30);
verify("Test6 3", r, 3);

// --------------------------------------------------------------------------------------------------
function Test7() {
    return arguments.length;
}

r = Test7();
verify("Test7", r, 0);

r = Test7(10, 20);
verify("Test7", r, 2);

// --------------------------------------------------------------------------------------------------
var callCount = 5;
function Test8() {
    var calledCount = 0;
    write("Test8 : " + callCount);
    if (callCount != 0) {
        callCount = callCount - 1;
        calledCount = arguments.callee() + 1;
    }
    return calledCount;
}

r = Test8();
verify("Test8", r, 5);

// --------------------------------------------------------------------------------------------------

function Test9() {
    write(arguments.hasOwnProperty("length"));
    write(arguments[0] + " " + arguments[1] + " " + arguments.length);

    arguments.length = "test";

    write(arguments[0] + " " + arguments[1] + " " + arguments.length);
    write(arguments.hasOwnProperty("0"));
    write(arguments.hasOwnProperty("length"));

    return arguments.length;
}

r = Test9();
verify("Test9", r, "test");

r = Test9(10);
verify("Test9", r, "test");

r = Test9(10, 20);
verify("Test9", r, "test");

r = Test9(10, 20, 30);
verify("Test9", r, "test");

// --------------------------------------------------------------------------------------------------
// Test arguments for-in enumeration

function dump_props(obj) {
    var s = "";
    for (p in obj) {
        if (!s) {
            s = "[" + p + ": " + obj[p];
        } else {
            s += ", " + p + ": " + obj[p];
        }
    }

    if (s) {
        s += "]";
    } else {
        s = "[]";
    }
    return s;
}

//
// simply dump args
//
var dump_args = function (a, b) {
    return dump_props(arguments);
}

verify("Test10.1",
       dump_args(),
       "[]");

verify("Test10.1",
       dump_args(13),
       "[0: 13]");

verify("Test10.1",
       dump_args(13, 24),
       "[0: 13, 1: 24]");

verify("Test10.1",
       dump_args(13, 24, "string", true),
       "[0: 13, 1: 24, 2: string, 3: true]");

//
// make some changes then dump
//
dump_args = function (a, b) {
    arguments[0] = 98;
    b = 54;
    arguments[8] = false;
    arguments[10] = 21;
    arguments.foo = 'bar';
    return dump_props(arguments);
}

verify("Test10.2",
       dump_args(),
       "[0: 98, 8: false, 10: 21, foo: bar]");

verify("Test10.2",
       dump_args(13),
       "[0: 98, 8: false, 10: 21, foo: bar]");

verify("Test10.2",
       dump_args(13, 24),
       "[0: 98, 1: 54, 8: false, 10: 21, foo: bar]");

verify("Test10.2",
       dump_args(13, 24, "string", true),
       "[0: 98, 1: 54, 2: string, 3: true, 8: false, 10: 21, foo: bar]");

//
// delete and dump
//
var get_args = function (a0, a1, a2, a3, a4) {
    return arguments;
}

var delete_args_test = function (args, exp_del, exp_set) {
    delete args[1];
    delete args[3];
    verify("Test10.3.1", dump_props(args), exp_del);
    args.foo = "bar";
    args[6] = "arg6";
    args[2] = "arg2";
    args[1] = "arg1";
    verify("Test10.3.2", dump_props(args), exp_set);
}

delete_args_test(get_args(),
                 "[]",
                 "[1: arg1, 2: arg2, 6: arg6, foo: bar]"
                 );

delete_args_test(get_args(13),
                 "[0: 13]",
                 "[0: 13, 1: arg1, 2: arg2, 6: arg6, foo: bar]"
                 );

delete_args_test(get_args(13, 24),
                 "[0: 13]", //[1] deleted
                 "[0: 13, 1: arg1, 2: arg2, 6: arg6, foo: bar]"
// [1] set after formal args
                 );

delete_args_test(get_args(13, 24, "string", true),
                 "[0: 13, 2: string]", //[1][3] deleted
                 "[0: 13, 2: arg2, 1: arg1, 6: arg6, foo: bar]"
//[1] set after formal args [0][2]
                 );

function Test11() {
    var _t11;

    function inner11() {
        arguments.valueOf = function() { _t11 = this; }

        var x = arguments++;
        return x;
    }

    inner11("sentinel");
    verify("Test11 args", _t11[0], "sentinel");
}

Test11();

function Test12() {
    function inner12() {
        throw arguments;
    }

    try {
        inner12("sentinel");
    } catch (e) {
        verify("Test12 args", e[0], "sentinel");
    }
}

Test12();

function Test13() {
    var _t13;

    function inner13() {
        arguments.capture = function() { _t13 = this }

        with(arguments)
            capture();
    }

    inner13("sentinel");
    verify("Test13 args", _t13[0], "sentinel");
}

Test13();

function TestBuiltInProperty(propName) {
    write("");
    write("-------Testing built-in " + propName + " Implementation----");
    verify("HasOwnProperty() test", arguments.hasOwnProperty(propName), true);
    verify("IsEnumerable() test", arguments.propertyIsEnumerable(propName), false);
    arguments[propName] = 40;
    verify("Overriding value test", arguments[propName], 40);
    delete arguments[propName];
    verify("HasOwnProperty() after deletion test", arguments.hasOwnProperty(propName), false);
    verify("Value after deletion test", arguments[propName], undefined);
}

TestBuiltInProperty("callee");
TestBuiltInProperty("length");

function Test15() {
    var count = 0;

    arguments.length += 10;
    arguments.length -= 10;

    arguments[0] = 100;
    arguments[1] = 200;

    for (var i in arguments) {
        count++;
    }

    verify("Test15", count, 2);
}

Test15();

function Test16() {
    verify("Test16", arguments.length, 3);
    function Test16_inner() {
        eval("");
    }
}

Test16(1, 2, 3);

function Test17(a) {
    verify("Test17.1", arguments.length, 1);
    verify("Test17.2", a, "Feb20");
    function Test17_inner() {
        eval("");
    }
}

Test17("Feb20");

// Test that changing arguments in one function is reflected in the caller

function Test18_Helper() {
    Test18_Helper.caller.arguments.Test18_Value = "Test 18 Value";
}

function Test18() {
    verify("Test18.1", arguments.Test18_Value, undefined);
    Test18_Helper();
    verify("Test18.2", arguments.Test18_Value, "Test 18 Value");
}

Test18();

function Test19(flag)
{
    if (flag)
    {
        write("test19 called");
    }
    else
    {
        Test19_Helper();
    }
}

function Test19_Helper()
{
    arguments.callee.caller(true);
}
Test19();

// --------------------------------------------------------------------------------------------------

if (errorCount > 0) {
    write(errorCount + " Tests ");
    throw new Error(errorCount);
}
