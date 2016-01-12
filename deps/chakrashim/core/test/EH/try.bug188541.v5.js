//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo() {
    try {
        throw "foo error";
    } catch (e) {
        WScript.Echo("Caught e=" + e);
        {
            let e = 10;
            WScript.Echo("Caught e=" + e);
        }

        WScript.Echo("Caught e=" + e);
    }
}

function foo2() {
    try {
        throw "foo error";
    } catch (e) {
        WScript.Echo("Caught e=" + e);
        var e = 10;
        WScript.Echo("Caught e=" + e);
    }
}

function foo3() {
    try {
        throw "foo error";
    } catch (e) {
        WScript.Echo("Caught e=" + e);
        var e = 10;
        {
            try {
                e = 0;
            }
            catch(err) {
                WScript.Echo("Caught expected err=" + err);
            }

            let e = 20;
            WScript.Echo("Caught e=" + e);
        }

        WScript.Echo("Caught e=" + e);
    }
}

function foo4() {
    try {
        throw "foo error";
    } catch (e) {
        WScript.Echo("Caught e=" + e);
        {
            let e = 20;
            WScript.Echo("Caught e=" + e);
        }

        WScript.Echo("Caught e=" + e);
    }
}

function foo5() {
    try {
        throw "foo error";
    } catch (e) {
        WScript.Echo("Caught e=" + e);
        e = 10;
        {
            try {
                e = 0;
            }
            catch(err) {
                WScript.Echo("Caught expected err=" + err);
            }
            let e = 20;
            WScript.Echo("Caught e=" + e);
        }

        WScript.Echo("Caught e=" + e);
    }
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

WScript.Echo("foo5():");
foo5();
WScript.Echo("");

WScript.Echo("PASSED");
