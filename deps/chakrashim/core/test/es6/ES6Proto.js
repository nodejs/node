//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Tests for Object.setPrototypeOf and Object#__proto__ ES6 behavior

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var pd = Object.getOwnPropertyDescriptor(Object.prototype, '__proto__');
var __proto__set = pd.set;
var __proto__get = pd.get;

var tests = [
    {
        name: "Sanity base cases",
        body: function() {
            assert.areEqual('function', typeof __proto__set, "Object#__proto__ is an accessor property with set method");
            assert.areEqual('function', typeof __proto__get, "Object#__proto__ is an accessor property with get method");
        }
    },
    {
        name: "Error conditions for Object#__proto__",
        body: function () {
            assert.throws(function() { __proto__set.call(); }, TypeError, "set Object#__proto__ throws if this argument is not passed", "Object.prototype.__proto__: 'this' is not an Object");
            assert.throws(function() { __proto__set.call(undefined); }, TypeError, "set Object#__proto__ throws if this argument is undefined", "Object.prototype.__proto__: 'this' is not an Object");
            assert.throws(function() { __proto__set.call(null); }, TypeError, "set Object#__proto__ throws if this argument is null", "Object.prototype.__proto__: 'this' is not an Object");

            assert.throws(function() { __proto__get.call(); }, TypeError, "get Object#__proto__ throws if this argument is not passed", "Object.prototype.__proto__: 'this' is not an Object");
            assert.throws(function() { __proto__get.call(undefined); }, TypeError, "get Object#__proto__ throws if this argument is undefined", "Object.prototype.__proto__: 'this' is not an Object");
            assert.throws(function() { __proto__get.call(null); }, TypeError, "get Object#__proto__ throws if this argument is null", "Object.prototype.__proto__: 'this' is not an Object");
        }
    },
    {
        name: "Cases where Object#__proto__ shouldn't change [[Prototype]]",
        body: function () {
            var p = {};
            var o = Object.create(p);

            assert.areEqual(undefined, __proto__set.call(o), "set Object#__proto__ returns undefined if the proto argument is not passed");
            assert.areEqual(p, __proto__get.call(o), "[[Prototype]] slot of o was not changed");
            assert.areEqual(undefined, __proto__set.call(o, undefined), "set Object#__proto__ returns undefined if the proto argument is undefined");
            assert.areEqual(p, __proto__get.call(o), "[[Prototype]] slot of o was not changed");
            assert.areEqual(undefined, __proto__set.call(o, 5), "set Object#__proto__ returns undefined if the proto argument is non-object");
            assert.areEqual(p, __proto__get.call(o), "[[Prototype]] slot of o was not changed");

            var n = 5;
            assert.areEqual(undefined, __proto__set.call(n, {}), "set Object#__proto__ returns undefined if the this argument is non-object when proto argument is supplied");
            assert.areEqual(Number.prototype, __proto__get.call(n), "[[Prototype]] slot of n was not changed");
        }
    },
    {
        name: "Simple validation of Object#__proto__",
        body: function () {
            var p = {};
            var o = Object.create(p);

            assert.areEqual(undefined, __proto__set.call(o, null), "set Object#__proto__ returns undefined if the proto argument is null");
            assert.areEqual(null, __proto__get.call(o), "[[Prototype]] slot of o was changed to null");

            assert.areEqual(undefined, __proto__set.call(o, p), "set Object#__proto__ returns undefined if the proto argument is object");
            assert.areEqual(p, __proto__get.call(o), "[[Prototype]] slot of o was changed to p");
        }
    },
    {
        name: "Error conditions for Object.setPrototypeOf/getPrototypeOf",
        body: function () {
            assert.throws(function() { Object.setPrototypeOf(); }, TypeError, "Object.setPrototypeOf throws when called with no arguments", "Object.setPrototypeOf: argument is not an Object");
            assert.throws(function() { Object.setPrototypeOf(undefined); }, TypeError, "Object.setPrototypeOf throws when object argument is undefined", "Object.setPrototypeOf: argument is not an Object");
            assert.throws(function() { Object.setPrototypeOf(null); }, TypeError, "Object.setPrototypeOf throws when object argument is null", "Object.setPrototypeOf: argument is not an Object");

            assert.throws(function() { Object.setPrototypeOf({}); }, TypeError, "Object.setPrototypeOf throws when proto is not passed", "Object.setPrototypeOf: argument is not an Object and is not null");
            assert.throws(function() { Object.setPrototypeOf({}, undefined); }, TypeError, "Object.setPrototypeOf throws when proto is undefined", "Object.setPrototypeOf: argument is not an Object and is not null");
            assert.throws(function() { Object.setPrototypeOf({}, 5); }, TypeError, "Object.setPrototypeOf throws when proto is not object", "Object.setPrototypeOf: argument is not an Object and is not null");

            assert.throws(function() { Object.getPrototypeOf(); }, TypeError, "Object.getPrototypeOf throws when argument is not passed", "Object.getPrototypeOf: argument is not an Object");
            assert.throws(function() { Object.getPrototypeOf(undefined); }, TypeError, "Object.getPrototypeOf throws when argument is undefined", "Object.getPrototypeOf: argument is not an Object");
            assert.throws(function() { Object.getPrototypeOf(null); }, TypeError, "Object.getPrototypeOf throws when argument is null", "Object.getPrototypeOf: argument is not an Object");
        }
    },
    {
        name: "Object.setPrototypeOf used on non-object argument doesn't change [[Prototype]]",
        body: function () {
            var n = 5;
            assert.areEqual(5, Object.setPrototypeOf(n, {}), "Object.setPrototypeOf returns first argument if argument is non-object when proto argument is supplied");
            assert.areEqual(Number.prototype, Object.getPrototypeOf(n), "[[Prototype]] slot of n was not changed");
        }
    },
    {
        name: "Simple validation of Object.setPrototypeOf",
        body: function () {
            var p = {};
            var o = Object.create(p);

            assert.areEqual(o, Object.setPrototypeOf(o, null), "Object.setPrototypeOf returns o if the proto argument is null");
            assert.areEqual(null, Object.getPrototypeOf(o), "[[Prototype]] slot of o was changed to null");

            assert.areEqual(o, Object.setPrototypeOf(o, p), "Object.setPrototypeOf returns o if the proto argument is object");
            assert.areEqual(p, Object.getPrototypeOf(o), "[[Prototype]] slot of o was changed to p");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
