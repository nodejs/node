//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    function test0a(o) {
        var o2 = o;
        var a = o.q;
        o2.p = 0;
        o.p = 1;
    }
    var o = {};
    test0a(o);
    return objectToString(o);
}
WScript.Echo("test0: " + test0());

function test1() {
    function test1a(o, o2) {
        if(o.p) {
            o2.r = 0;
            o2.s = 0;
            o2.t = 0;
        }
        o.q = 1;
    }
    var o = { p: 0, q: 0 };
    var o2 = { p: 1, q: 0 };
    test1a(o, o);
    test1a(o2, o2);
    return objectToString(o2);
}
WScript.Echo("test1: " + test1());

function test2() {
    function test2a(o, o2) {
        if(o.p) {
            delete o2.q;
        }
        o.q = 1;
    }
    var o = { p: 0, q: 0 };
    var o2 = { p: 1, q: 0 };
    test2a(o, o);
    test2a(o2, o2);
    return objectToString(o2);
}
WScript.Echo("test2: " + test2());

function test3() {
    function test3a(o, o2) {
        if(o.p) {
            var p = "q";
            delete o2[p];
        }
        o.q = 1;
    }
    var o = { p: 0, q: 0 };
    var o2 = { p: 1, q: 0 };
    test3a(o, o);
    test3a(o2, o2);
    return objectToString(o2);
}
WScript.Echo("test3: " + test3());

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

function objectToString(o) {
    var s = "";
    for(var p in o)
        s += p + ": " + o[p] + ", ";
    if(s.length !== 0)
        s = s.substring(0, s.length - ", ".length);
    return "{" + s + "}";
}
