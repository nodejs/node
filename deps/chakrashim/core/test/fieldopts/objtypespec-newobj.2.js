//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Test the happy path of the new object optimization involving built-ins.
WScript.Echo("Test 01:");

function test01() {
    var o = new Array();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test01();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test01();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test01();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");


WScript.Echo("Test 02:");

var proto02 = { p: 1001, q: 1002 };

function test02() {
    var o = new Array(10);
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test02();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test02();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test02();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 03:");

var proto03 = { p: 1001, q: 1002 };

Array.prototype = proto03;

function test03() {
    var o = new Array();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test03();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test03();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test03();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 04:");

function SimpleObject04() {
    this.a = 1;
    this.b = 2;
    this.c = 3;
}

var proto04 = { p: 1001, q: 1002 };

SimpleObject04.prototype = proto04;

Array = SimpleObject04;

function test04() {
    var o = new Array();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test04();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test04();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test04();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 11:");

function test11() {
    var o = new Boolean();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test11();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test11();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test11();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 12:");

var proto12 = { p: 1001, q: 1002 };

function test12() {
    var o = new Boolean(true);
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test12();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test12();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test12();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");


WScript.Echo("Test 13:");

var proto13 = { p: 1001, q: 1002 };

Boolean.prototype = proto13;

function test13() {
    var o = new Boolean();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test13();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test13();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test13();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 14:");

function SimpleObject14() {
    this.a = 1;
    this.b = 2;
    this.c = 3;
}

var proto14 = { p: 1001, q: 1002 };

SimpleObject14.prototype = proto14;

Boolean = SimpleObject14;

function test14() {
    var o = new Boolean();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test14();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test14();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test14();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 21:");

function test21() {
    var o = new Number();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test21();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test21();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test21();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 22:");

var proto22 = { p: 1001, q: 1002 };

function test22() {
    var o = new Number(0);
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test22();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test22();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test22();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 23:");

var proto23 = { p: 1001, q: 1002 };

Number.prototype = proto23;

function test23() {
    var o = new Number();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test23();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test23();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test23();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 24:");

function SimpleObject24() {
    this.a = 1;
    this.b = 2;
    this.c = 3;
}

var proto24 = { p: 1001, q: 1002 };

SimpleObject24.prototype = proto24;

Number = SimpleObject24;

function test24() {
    var o = new Number();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test24();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test24();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test24();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 31:");

function test31() {
    var o = new String();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test31();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test31();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test31();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 32:");

var proto32 = { p: 1001, q: 1002 };

function test32() {
    var o = new String("text");
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test32();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test32();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test32();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 33:");

var proto33 = { p: 1001, q: 1002 };

String.prototype = proto33;

function test33() {
    var o = new String();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test33();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test33();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test33();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 34:");

function SimpleObject34() {
    this.a = 1;
    this.b = 2;
    this.c = 3;
}

var proto34 = { p: 1001, q: 1002 };

SimpleObject34.prototype = proto34;

String = SimpleObject34;

function test34() {
    var o = new String();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test34();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test34();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test34();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 41:");

function test41() {
    var o = new Date("2013/12/03");
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test41();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test41();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test41();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 42:");

var proto42 = { p: 1041, q: 1042 };

Date.prototype = proto42;

function test42() {
    var o = new Date("2013/12/03");
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test42();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test42();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test42();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 43:");

function SimpleObject43() {
    this.a = 1;
    this.b = 2;
    this.c = 3;
}
var proto43 = { p: 1041, q: 1042 };

SimpleObject43.prototype = proto43;

Date = SimpleObject43;

function test43() {
    var o = new Date();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test43();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test43();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test43();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 51:");

function test51() {
    var o = new Object();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test51();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test51();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test51();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 52:");

var proto52 = { p: 1041, q: 1042 };

function test52() {
    var o = new Object(proto52);
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test52();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test52();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test52();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 53:");

var proto53 = { p: 1041, q: 1042 };

Object.prototype = proto53;

function test53() {
    var o = new Object();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test53();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test53();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test53();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 54:");

function SimpleObject54() {
    this.a = 1;
    this.b = 2;
    this.c = 3;
}

var proto54 = { p: 1001, q: 1002 };

SimpleObject54.prototype = proto54;

Object = SimpleObject54;

function test54() {
    var o = new Object();
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test54();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test54();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test54();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");

WScript.Echo("Test 55:");

function SimpleObject55() {
    this.a = 1;
    this.b = 2;
    this.c = 3;
}

var proto55a = { p: 1041, q: 1042 };
var proto55b = { p: 1051, q: 1052 };

SimpleObject55.prototype = proto55a;

Object = SimpleObject55;

function test55() {
    var o = new Object(proto55b);
    o.x = 4;
    o.y = 5;
    o.z = 6;
    return o;
}

var o = test55();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test55();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");

o = test55();
WScript.Echo("o = " + o + " ({ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " })");
WScript.Echo("");
