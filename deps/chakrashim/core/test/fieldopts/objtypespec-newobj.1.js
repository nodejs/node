//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Test the happy path of the new object optimization involving script constructors.
WScript.Echo("Test 1:");
function SimpleObject1() {
    this.a = 1;
    this.b = 2;
    this.c = 3;
}

SimpleObject1.prototype = { p: 0 };

function test1() {
    var o = new SimpleObject1();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test1();
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

o = test1();
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

o = test1();
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");
WScript.Echo("");

WScript.Echo("Test 2:");
function SimpleObject2(a, b, c) {
    this.a = a;
    this.b = b;
    this.c = c;
}

SimpleObject2.prototype = { p: 0 };

function test2() {
    var o = new SimpleObject2(1, 2, 3);
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test2();
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

o = test2();
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

o = test2();
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");
WScript.Echo("");

WScript.Echo("Test 3:");
function SimpleObject3(a, b, c) {
    this.a = a;
    this.b = b;
    this.c = c;
}

SimpleObject3.prototype = { p: 0 };

function test3(a, b, c) {
    var o = new SimpleObject3();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test3(1, 2, 3);
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

o = test3(1, 2, 3);
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

o = test3(1, 2, 3);
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");
WScript.Echo("");

WScript.Echo("Test 4:");
function SimpleObject4(a, b, c) {
    this.a = a;
    this.b = b;
    this.c = c;
}

SimpleObject4.prototype = { p: 0 };

function test4() {
    var o = new SimpleObject4();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test4();
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

var o = test4();
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

o = test4();
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");
WScript.Echo("");

WScript.Echo("Test 5:");
function SimpleObject5(a, b, c) {
    var o = {};
    o.a = a;
    o.b = b;
    o.c = c;
    return o;
}

SimpleObject5.prototype = { p: 0 };

function test5() {
    var o = new SimpleObject5();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test5();
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

var o = test5();
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

o = test5();
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");
WScript.Echo("");

WScript.Echo("Test 6:");
function SimpleObject6(a, b, c) {
    return a + b + c;
}

SimpleObject6.prototype = { p: 0 };

function test6() {
    var o = new SimpleObject6();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test6();
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

var o = test6();
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

o = test6();
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");
WScript.Echo("");

WScript.Echo("Test 7:");
function SimpleObject7(a, b, c, condition) {
    this.a = condition ? a : -a;
    this.b = condition ? b : -b;
    this.c = condition ? c : -c;
}

SimpleObject7.prototype = { p: 0 };

function test7(a, b, c, condition) {
    var o = new SimpleObject7(a, b, c);
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test7(1, 2, 3, false);
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

o = test7(1, 2, 3, false);
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

o = test7(1, 2, 3, true);
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");
WScript.Echo("");

WScript.Echo("Test 8:");
function SimpleObject8(a, b, c) {
    this.a = a;
    this.b = b;
    this.c = c;
    throw this;
}

SimpleObject8.prototype = { p: 0 };

function test8(a, b, c) {
    var o = new SimpleObject8();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = {};

try {
    var o = test8(1, 2, 3);
} catch (ex) {
    WScript.Echo("Exception: " + ex);
}
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

try {
    o = test8(1, 2, 3);
} catch (ex) {
    WScript.Echo("Exception: " + ex);
}
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

try {
    o = test8(1, 2, 3);
} catch (ex) {
    WScript.Echo("Exception: " + ex);
}
WScript.Echo("o = { a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");
WScript.Echo("");
