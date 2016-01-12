//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// This test verifies that we correctly bail out if a constructor cache is invalidated as part
// of evaluating one of the arguments of the call to the constructor.

// This test invalidates the constructor cache by changing the constructor's prototype.
WScript.Echo("Test 1:");
function SimpleObject1() {
    this.x = 0;
}

SimpleObject1.prototype = { p: 10 };

function test1(forceBailout) {
    var o = new SimpleObject1(forceBailout ? SimpleObject1.prototype = { q: 11 } : 0);
    return o;
}

var o = test1(false);
WScript.Echo("{ p: " + o.p + ", q: " + o.q + ", x: " + o.x + " }");

var o = test1(false);
WScript.Echo("{ p: " + o.p + ", q: " + o.q + ", x: " + o.x + " }");

o = test1(true);
WScript.Echo("{ p: " + o.p + ", q: " + o.q + ", x: " + o.x + " }");

for (var i = 0; i < 10; i++) {
    o = test1(false);
    WScript.Echo("{ p: " + o.p + ", q: " + o.q + ", x: " + o.x + " }");
}
WScript.Echo("");

// This test invalidates the constructor cache guard by making one of the properties protected by the
// guard read-only.  The property in question is added in the constructor.
WScript.Echo("Test 2:");
function SimpleObject2() {
    this.x = 0;
}

SimpleObject2.prototype = { p: 10 };

function test2(forceBailout) {
    var o = new SimpleObject2(forceBailout ? Object.defineProperty(SimpleObject2.prototype, "x", { value: 12, writable: false }) : 0);
    return o;
}

var o = test2(false);
WScript.Echo("{ p: " + o.p + ", q: " + o.q + ", x: " + o.x + " }");

var o = test2(false);
WScript.Echo("{ p: " + o.p + ", q: " + o.q + ", x: " + o.x + " }");

o = test2(true);
WScript.Echo("{ p: " + o.p + ", q: " + o.q + ", x: " + o.x + " }");

for (var i = 0; i < 10; i++) {
    o = test2(false);
    WScript.Echo("{ p: " + o.p + ", q: " + o.q + ", x: " + o.x + " }");
}
WScript.Echo("");

// This test invalidates the constructor cache guard by making one of the properties protected by the
// guard read-only.  The property in question is added after the constructor
WScript.Echo("Test 3:");
function SimpleObject3() {
    this.x = 0;
}

SimpleObject3.prototype = { p: 10 };

function test3(forceBailout) {
    var o = new SimpleObject3(forceBailout ? Object.defineProperty(SimpleObject3.prototype, "y", { value: 12, writable: false }) : 0);
    o.y = 1;
    return o;
}

var o = test3(false);
WScript.Echo("{ p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

var o = test3(false);
WScript.Echo("{ p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

o = test3(true);
WScript.Echo("{ p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");

for (var i = 0; i < 10; i++) {
    o = test3(false);
    WScript.Echo("{ p: " + o.p + ", q: " + o.q + ", x: " + o.x + ", y: " + o.y + " }");
}

WScript.Echo("");
