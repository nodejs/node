//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

Object.prototype.prop0 = 100;
Object.prototype.method0 = function () { return 100; }

function SimpleObject() {
}

function test1a(o) {
    return o.prop0;
}
WScript.Echo(test1a(new SimpleObject()));
WScript.Echo(test1a(new SimpleObject()));
WScript.Echo(test1a(new SimpleObject()));
WScript.Echo(test1a(1));

function test1b(o) {
    return o.method0();
}
WScript.Echo(test1b(new SimpleObject()));
WScript.Echo(test1b(new SimpleObject()));
WScript.Echo(test1b(new SimpleObject()));
WScript.Echo(test1b(1));

function test2a(o) {
    return o.prop0;
}
WScript.Echo(test2a(new SimpleObject()));
WScript.Echo(test2a(new SimpleObject()));
WScript.Echo(test2a(new SimpleObject()));
WScript.Echo(test2a(0.5));

function test2b(o) {
    return o.method0();
}
WScript.Echo(test2b(new SimpleObject()));
WScript.Echo(test2b(new SimpleObject()));
WScript.Echo(test2b(new SimpleObject()));
WScript.Echo(test2b(0.5));

function test3a(o) {
    return o.prop0;
}
WScript.Echo(test3a(new SimpleObject()));
WScript.Echo(test3a(new SimpleObject()));
WScript.Echo(test3a(new SimpleObject()));
WScript.Echo(test3a(Math.max(0x5a827999, -262144)));

function test3b(o) {
    return o.method0();
}
WScript.Echo(test3b(new SimpleObject()));
WScript.Echo(test3b(new SimpleObject()));
WScript.Echo(test3b(new SimpleObject()));
WScript.Echo(test3b(Math.max(0x5a827999, -262144)));

function test4a(o) {
    return o.prop0;
}
WScript.Echo(test4a(new SimpleObject()));
WScript.Echo(test4a(new SimpleObject()));
WScript.Echo(test4a(new SimpleObject()));
WScript.Echo(test4a(Math.max(0.5, -262144)));

function test4b(o) {
    return o.method0();
}
WScript.Echo(test4b(new SimpleObject()));
WScript.Echo(test4b(new SimpleObject()));
WScript.Echo(test4b(new SimpleObject()));
WScript.Echo(test4b(Math.max(0.5, -262144)));
