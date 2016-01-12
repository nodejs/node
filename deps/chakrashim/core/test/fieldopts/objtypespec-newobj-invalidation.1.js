//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Switches: -bgJit- -maxInterpretCount:2

// Test 1: Invalidate constructor cache by changing the constructor's prototype property.
WScript.Echo("Test 1:");

var SimpleObject1 = function () {
    this.a = 0;
    this.b = 1;
}

var proto1a = { p: 100 };
var proto1b = { p: 200 };

SimpleObject1.prototype = proto1a;

var test1 = function () {
    var o = new SimpleObject1();
    o.x = 10;
    o.y = 11;
    return o;
}

var oa1 = new Array();

// Run twice in the interpreter to populate add property caches and kick off JIT compilation.
oa1.push(test1());
oa1.push(test1());

// Run once along the happy path.
oa1.push(test1());

// Now change the prototype object to invalidate the constructor cache and cause a bailout.
SimpleObject1.prototype = proto1b;

oa1.push(test1());

// Run a few more times (bailing out every time) to force a re-JIT.  After re-JIT the optimization
// should be disabled because the constructor cache is now marked as polymorphic.

oa1.push(test1());
oa1.push(test1());
oa1.push(test1());
oa1.push(test1());
oa1.push(test1());

for (var i = 0; i < oa1.length; i++) {
    var o = oa1[i];
    WScript.Echo("oa1[" + i + "]: " + "{ a: " + o.a + ", b: " + o.b + ", p: " + o.p + ", x: " + o.x + ", y: " + o.y + " }");
}
WScript.Echo("");

// Test 2: Invalidate the constructor cache by making one of the properties added in the
// constructor read-only
WScript.Echo("Test 2:");

var SimpleObject2 = function () {
    this.a = 0;
    this.b = 1;
    this.c = 2;
}

var proto2a = { p: 100 };

SimpleObject2.prototype = proto2a;

var test2 = function () {
    var o = new SimpleObject2();
    o.x = 10;
    o.y = 11;
    return o;
}

var oa2 = new Array();

// Run twice in the interpreter to populate add property caches and kick off JIT compilation.
oa2.push(test2());
oa2.push(test2());

// Run once along the happy path.
oa2.push(test2());

// Now invalidate the constructor cache by making one of the properties added in the constructor
// read-only.
Object.defineProperty(proto2a, "a", { value: 101, writable: false });

oa2.push(test2());

// Run a few more times (bailing out every time) to force a re-JIT.  After re-JIT, we should no longer
// have the offending property access object type specialized, and thus protected by the ctor guard.
oa2.push(test2());
oa2.push(test2());
oa2.push(test2());
oa2.push(test2());
oa2.push(test2());

for (var i = 0; i < oa2.length; i++) {
    var o = oa2[i];
    WScript.Echo("oa2[" + i + "]: " + "{ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", p: " + o.p + ", x: " + o.x + ", y: " + o.y + " }");
}
WScript.Echo("");

// Test 3: Invalidate the constructor cache by making one of the properties added after the
// constructor read-only
WScript.Echo("Test 3:");

var SimpleObject3 = function () {
    this.a = 0;
    this.b = 1;
}

var proto3a = { p: 100 };

SimpleObject3.prototype = proto3a;

var test3 = function () {
    var o = new SimpleObject3();
    o.x = 10;
    o.y = 11;
    return o;
}

var oa3 = new Array();

// Run twice in the interpreter to populate add property caches and kick off JIT compilation.
oa3.push(test3());
oa3.push(test3());

// Run once along the happy path.
oa3.push(test3());

// Now invalidate the constructor cache by making one of the properties added in the constructor
// read-only.
Object.defineProperty(proto3a, "x", { value: 102, writable: false });

oa3.push(test3());

// Run a few more times (bailing out every time) to force a re-JIT.  After re-JIT, we should no longer
// have the offending property access object type specialized, and thus protected by the ctor guard.
oa3.push(test3());
oa3.push(test3());
oa3.push(test3());
oa3.push(test3());
oa3.push(test3());

for (var i = 0; i < oa3.length; i++) {
    var o = oa3[i];
    WScript.Echo("oa3[" + i + "]: " + "{ a: " + o.a + ", b: " + o.b + ", p: " + o.p + ", x: " + o.x + ", y: " + o.y + " }");
}
WScript.Echo("");

// Test 4: Invalidate constructor cache by changing the constructor's prototype property.
WScript.Echo("Test 4:");

var SimpleObject4 = function () {
    this.a = this.p + 0;
    this.b = this.p + 1;
    this.c = this.p + 2;
    this.d = this.p + 3;
    this.e = this.p + 4;
    this.f = this.p + 5;
    this.g = this.p + 6;
    this.h = this.p + 7;
    this.i = this.p + 8;
    this.j = this.p + 9;
}

var proto4a = { p: 100 };
var proto4b = Object.create(proto4a);

SimpleObject4.prototype = proto4b;

var test4 = function () {
    var o = new SimpleObject4();
    o.x = o.p + 10;
    o.y = o.p + 11;
    return o;
}

var oa4 = new Array();

// Run twice in the interpreter to populate add property caches and kick off JIT compilation.
oa4.push(test4());
oa4.push(test4());

// Run once along the happy path.
oa4.push(test4());

// Now change the prototype object to invalidate the constructor cache and cause a bailout.
proto4b.p = 200;

oa4.push(test4());

// Run a few more times (bailing out every time) to force a re-JIT.  After re-JIT, we should no longer
// have the offending property access object type specialized, and thus protected by the ctor guard.
// This time all the other property operations should remain object type specialized, because the type
// of the object created is not affected by this change to the prototype chain.
oa4.push(test4());
oa4.push(test4());
oa4.push(test4());
oa4.push(test4());
oa4.push(test4());

for (var i = 0; i < oa4.length; i++) {
    var o = oa4[i];
    WScript.Echo("oa4[" + i + "]: " + "{ a: " + o.a + ", b: " + o.b + ", c: " + o.c + ", i: " + o.i + ", j: " + o.j + ", p: " + o.p + ", x: " + o.x + ", y: " + o.y + " }");
}
WScript.Echo("");
