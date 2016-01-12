//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function globalFixedFunction1() {
    WScript.Echo("globalFixedFunction1: original");
}

var globalFixedFunction2 = function () {
    WScript.Echo("globalFixedFunction2: original");
}

function testGlobal() {
    globalFixedFunction1();
    globalFixedFunction2();
}

WScript.Echo("Testing the global object:");

testGlobal();

testGlobal();

globalFixedFunction1 = function () {
    WScript.Echo("globalFixedFunction1: overwritten");
}

globalFixedFunction2 = function () {
    WScript.Echo("globalFixedFunction2: overwritten");
}

testGlobal();

WScript.Echo();


WScript.Echo("Testing object literal:");

var objectLiteral = {
    unique1: 0,
    x: 0,
    y: 1,
    add: function () {
        return (this.x + this.y) + " (original)";
    },
    subtract: function () {
        return (this.x - this.y) + " (original)";
    }
}

function testObjectLiteral() {
    WScript.Echo("x + y = " + objectLiteral.add());
    WScript.Echo("x - y = " + objectLiteral.subtract());
}

testObjectLiteral();

testObjectLiteral();

objectLiteral.add = function () {
    return (this.x + this.y) + " (overwritten)";
}

testObjectLiteral();

WScript.Echo();


WScript.Echo("Testing Object.defineProperty with accessors:");

var object = {};
Object.defineProperty(object, "x", { get: function() { return "0 (original)"; }, configurable: true });

function testObjectDefineProperty() {
    WScript.Echo("x = " + object.x);
}

testObjectDefineProperty();

testObjectDefineProperty();

Object.defineProperty(object, "x", { get: function () { return "1 (overwritten)"; } });

testObjectDefineProperty();

WScript.Echo();


WScript.Echo("Testing the Math object:");

Math.identity = function (value) {
    return value;
}

function testMathObject() {
    WScript.Echo("Math.sin(Math.PI) = " + Math.sin(Math.PI));
    WScript.Echo("Math.identity(Math.PI) = " + Math.identity(Math.PI));
}

testMathObject();

testMathObject();

Math.identity = function (value) {
    return -value;
}

testMathObject();

Math.sin = function (value) {
    return -value;
}

testMathObject();

WScript.Echo();


WScript.Echo("Testing the Object constructor:");

Object.identity = function (value) {
    return value;
}

function testObjectConstructor() {
    var o = {};
    Object.seal(o);
    WScript.Echo("Object.identity(o) = " + Object.identity(o));
    WScript.Echo("Object.isSealed(o) = " + Object.isSealed(o));
}

testObjectConstructor();

testObjectConstructor();

Object.identity = function (value) {
    return "I don't know you anymore...";
}

testObjectConstructor();

Object.seal = function (object) {
    return false;
}

testObjectConstructor();

Object.isSealed = function (object) {
    return "With the magic of JavaScript I pronounce you sealed!";
}

testObjectConstructor();

WScript.Echo();
