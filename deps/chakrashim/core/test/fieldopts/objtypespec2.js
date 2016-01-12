//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

Object.prototype.prop0 = 100;
Object.prototype.method0 = function () { return 100; }

function test1a() {
    var obj0 = Math.max(0x5a827999, -262144);
    return obj0.prop0;
}
WScript.Echo(test1a());
WScript.Echo(test1a());
WScript.Echo(test1a());

function test1b() {
    var obj0 = Math.max(0x5a827999, -262144);
    return obj0.method0();
}
WScript.Echo(test1b());
WScript.Echo(test1b());
WScript.Echo(test1b());

function test1c() {
    return Math.max(0x5a827999, -262144).method0();
}
WScript.Echo(test1c());
WScript.Echo(test1c());
WScript.Echo(test1c());

function test2a() {
    var obj0 = Math.max(0.5, -262144);
    return obj0.method0();
}
WScript.Echo(test2a());
WScript.Echo(test2a());
WScript.Echo(test2a());

function test2b() {
    var obj0 = Math.max(0.5, -262144);
    return obj0.method0();
}
WScript.Echo(test2b());
WScript.Echo(test2b());
WScript.Echo(test2b());

function test2c() {
    return Math.max(0.5, -262144).method0();
}
WScript.Echo(test2c());
WScript.Echo(test2c());
WScript.Echo(test2c());

function test3a() {
    var obj0 = { prop0: 1 };
    var t = obj0.prop0;

    var obj0 = 1;

    // The inline cache here is shared with the one for prop0 above, so it will be populated,
    // and we will do object type specialization.
    return obj0.prop0;
}
WScript.Echo(test3a());
WScript.Echo(test3a());
WScript.Echo(test3a());

function test3b() {
    var obj0 = { method0: 1 };
    var t = obj0.method0;

    var obj0 = 1;

    // The inline cache here is shared with the one for prop0 above, so it will be populated,
    // and we will do object type specialization.
    return obj0.method0();
}
WScript.Echo(test3b());
WScript.Echo(test3b());
WScript.Echo(test3b());
