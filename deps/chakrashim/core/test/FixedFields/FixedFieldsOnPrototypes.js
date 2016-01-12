//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.Echo("Testing object literal as prototype:");

var objectLiteralProto = {
    add: function () {
        return (this.x + this.y) + " (original)";
    },
    subtract: function () {
        return (this.x - this.y) + " (original)";
    }
}

var objectWithObjectLiteralAsProto = Object.create(objectLiteralProto);
objectWithObjectLiteralAsProto.x = 0;
objectWithObjectLiteralAsProto.y = 1;

function testObjectLiteralAsProto() {
    WScript.Echo("x + y = " + objectWithObjectLiteralAsProto.add());
    WScript.Echo("x - y = " + objectWithObjectLiteralAsProto.subtract());
}

testObjectLiteralAsProto();

testObjectLiteralAsProto();

objectLiteralProto.subtract = function () {
    return (this.x - this.y) + " (overwritten)";
}

testObjectLiteralAsProto();

objectLiteralProto.add = function () {
    return (this.x + this.y) + " (overwritten)";
}

testObjectLiteralAsProto();

var objectLiteralProto = {
    add: function () {
        return (this.x + this.y) + " (original)";
    },
    subtract: function (object) {
        return (object.x - object.y) + " (original)";
    }
}

var objectWithObjectLiteralAsProto = Object.create(objectLiteralProto);
objectWithObjectLiteralAsProto.x = 0;
objectWithObjectLiteralAsProto.y = 1;

function testObjectLiteralProto() {
    WScript.Echo("x + y = " + objectWithObjectLiteralAsProto.add());
    // Calling a prototype method directly on the prototype object.
    WScript.Echo("x - y = " + objectLiteralProto.subtract(objectWithObjectLiteralAsProto));
}

testObjectLiteralProto();

testObjectLiteralProto();

objectLiteralProto.subtract = function (object) {
    return (object.x - object.y) + " (overwritten)";
}

testObjectLiteralProto();

WScript.Echo();


WScript.Echo("Testing the String prototype:");

String.prototype.identity = function (value) {
    return value;
}

function testStringPrototype() {
    var s = "I'm a string, I believe.";
    WScript.Echo("s.identity() = " + s.identity(s));
    WScript.Echo("s.indexOf(\"s\") = " + s.indexOf("s"));
}

testStringPrototype();

testStringPrototype();

String.prototype.indexOf = function (searchString, position) {
    return -1;
}

testStringPrototype();

String.prototype.identity = function (value) {
    return "To me you're just a number.";
}

testStringPrototype();

WScript.Echo();


WScript.Echo("Testing the global object as prototype:");

function globalFixedFunction1() {
    WScript.Echo("globalFixedFunction1: original");
}

var globalFixedFunction2 = function () {
    WScript.Echo("globalFixedFunction2: original");
}

function createObjectWithGlobalAsProto() {
    return Object.create(this);
}

var objectWithGlobalAsProto = createObjectWithGlobalAsProto();

function testGlobalAsProto() {
    objectWithGlobalAsProto.globalFixedFunction1();
    objectWithGlobalAsProto.globalFixedFunction2();
}

testGlobalAsProto();

testGlobalAsProto();

globalFixedFunction1 = function () {
    WScript.Echo("globalFixedFunction1: overwritten");
}

globalFixedFunction2 = function () {
    WScript.Echo("globalFixedFunction2: overwritten");
}

testGlobalAsProto();

WScript.Echo();
