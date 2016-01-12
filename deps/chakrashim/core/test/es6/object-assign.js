//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Object.is(x,y) API extension tests -- verifies the API shape and basic functionality

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Object.assign should exist and have length 2",
        body: function () {
            assert.isTrue(Object.hasOwnProperty('assign'), "Object should have an assign method");
            assert.areEqual(2, Object.assign.length, "assign method takes two arguments");
        }
    },
    {
        name: "Object.assign(o1, ...) should call ToObject on its sources",
        body: function () {
            assert.areEqual({}, Object.assign(1), "Call ToObject on target");
            assert.areEqual({}, Object.assign(1, 2, 3), "Call ToObject on target and sources");
            assert.areEqual({}, Object.assign({}, 2, 3), "Call ToObject on sources");
        }
    },

    {
        name: "Object.assign(object, null) and Object.assign(object, undefined) are no-op",
        body: function () {
            assert.areEqual({ a: 1 }, Object.assign({ a: 1 }, undefined), "Passing undefined as nextSource in assign method is a no-op");
            assert.areEqual({ a: 1 }, Object.assign({ a: 1 }, null), "Passing null as nextSource in assign method is a no-op");
        }
    },
    {
        name: "Object.assign(target) returns target",
        body: function () {
            var o = {x:10, y:20, z:30}
            assert.isTrue(o === Object.assign(o), "Object.assign(target) should return the same object");
            assert.isTrue(o === Object.assign(o, { a: 1 }), "Object.assign(target, source) should return the same object");
            assert.isTrue(o === Object.assign(o, { b: 2 }, { c: 3 }), "Object.assign(target, source1, source2) should return the same object");
        }
    },
    {
        name: "Object.assign({}, object) clone should work",
        body: function () {
            assert.areEqual({ a: 1 }, Object.assign({}, { a: 1 }), "Create a clone of a object");
            assert.areEqual([1], Object.assign({}, [1]), "Create a clone of a array");
            assert.areEqual(new String("hello"), Object.assign({}, new String("hello")), "Create a clone of a string");
        }
    },
    {
        name: "Object.assign(o1, o2, o3) Merging objects should work",
        body: function () {
            assert.areEqual({ a: 1, b: 2}, Object.assign({ a: 1 }, { b: 2 }), "Merging 2 plain objects should work");
            assert.areEqual({ a: 1, b: 2, c: 3 }, Object.assign({ a: 1 }, { b: 2 }, { c: 3 }), "Merging 3 plain objects should work");
            assert.areEqual({ a: 1, b: 2, c: 3, d: 4 }, Object.assign({ a: 1 }, { b: 2 }, { c: 3 }, { d: 4 }), "Merging 4 plain objects should work");
        }
    },
    {
        name: "Object.assign(o1, o2, o3) overlapping property names should preference last set",
        body: function () {
            assert.areEqual({ a: 2 }, Object.assign({ a: 1 }, { a: 2 }), "Merging 2 plain objects with the overlapping property name should work");
            assert.areEqual({ a: 1, b: 2, c: 3, z: 3 }, Object.assign({ a: 1, z: 1 }, { b: 2, z: 2 }, { c: 3, z: 3 }), "Merging 3 plain objects with overlapping property name should work");
        }
    },
    {
        name: "Object.assign(o1, o2) throw exception if GetOwnProperty for any enumerable property in o2 if enumeration fails",
        body: function () {
            try
            {
                Object.assign({}, { get x() { throw "xyz"; } });
                assert.fail("Exception is not thrown when GetOwnProperty fails");
            }
            catch (e)
            {
            }
        }
    },
    {
        name: "Object.assign(o1, o2) exception if SetProperty for any enumerable property in o2 if enumeration fails",
        body: function () {
            try {
                Object.assign({ set x(a) { throw "xyz"; } }, { x: 10 });
                assert.fail("Exception is not thrown when SetProperty fails");
            }
            catch (e) {
            }
        }
    },
    {
        name: "Object.assign(o1, o2, o2) should work on undefined values",
        body: function () {
            assert.areEqual({x: undefined}, Object.assign({}, {x : 40}, { x: undefined }), "Undefined values for the property keys are propagated");
        }
    },
    {
        name: "Object.assign(o1, o2, o2) ignore non-enumerable values",
        body: function () {
            var o2 = { y: 40 };
            Object.defineProperty(o2, "x", {value: 50, enumerable:false})
            assert.areEqual({y: 40}, Object.assign({}, o2), "Non enumerable values are not ignored");
        }
    },
    {
        name: "Object.assign(Object.create(Array.prototype), o2) create pattern",
        body: function () {
            assert.areEqual({ x: 10, y: 40 }, Object.assign(Object.create(Array.prototype), { x: 10, y: 40 }), "Object.create pattern should work ");
        }
    },
    {
        name: "Object.assign(o1, o2) works for symbols",
        body: function () {
            var o2 = {};
            var y = Symbol("y");
            o2[y] = 10;
            assert.isTrue((Object.assign({}, o2))[y] === 10, "Symbols are assigned to target object ");
        }
    },
    {
        name: "Object.assign(o1, o2) works for index properties",
        body: function () {
            assert.areEqual([1, 2, 3], Object.assign([], [1, 2, 3]), "Indexed properties are assigned to the target object");
        }
    },
    {
        name: "Object.assign(o1, o2) exception if SetProperty fails due to preventExtentions on o1",
        body: function () {
                assert.throws((function() { 'use strict'; var o = Object.preventExtensions([,0]);Object.assign(o,'xo');}), TypeError, "Invalid operand to 'Object.assign': Object expected");
        }
    },
    {
        name: "OS Bug 3080673: Object.assign(o1, o2) exception if SetProperty fails due to non-writable on o1 when o1's value is a String",
        body: function () {
            assert.throws((function() {
                    var o1 = "aaa";
                    var o2 = "babbb";
                    Object.assign(o1, o2);
            }), TypeError, "Exception is not thrown when SetProperty fails", "Cannot modify non-writable property '0'");
        }
    },

];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
