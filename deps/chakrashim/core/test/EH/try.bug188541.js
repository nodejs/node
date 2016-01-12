//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo() {
    try {
        throw "foo error";
    } catch (e) {
        var e = 10;

        WScript.Echo("Caught e=" + e);
    }
}

function foo2() {
    var e;
    WScript.Echo("On entry: e=" + e);

    try {
        throw "foo error";
    } catch (e) {
        var e = 10;

        WScript.Echo("Caught e=" + e);
    }

    WScript.Echo("On exit: e=" + e);
}

function foo3() {
    var e;
    WScript.Echo("On entry: e=" + e);

    try {
        throw "foo error";
    } catch (e) {
        function err(e) {
            var e = 10;

            WScript.Echo("Caught e=" + e);
        }
        err(e);
    }

    WScript.Echo("On exit: e=" + e);
}

function foo4() {
    var e;
    WScript.Echo("On entry: e=" + e);

    try {
        throw "foo error";
    } catch (e) {
        var e = 10;
        {
            var e = 20;

            WScript.Echo("Caught e=" + e);
        }
        WScript.Echo("Caught e=" + e);
    }

    WScript.Echo("On exit: e=" + e);
}

WScript.Echo("foo():");
foo();
WScript.Echo("");

WScript.Echo("foo2():");
foo2();
WScript.Echo("");

WScript.Echo("foo3():");
foo3();
WScript.Echo("");

WScript.Echo("foo4():");
foo4();
WScript.Echo("");

WScript.Echo("PASSED");
