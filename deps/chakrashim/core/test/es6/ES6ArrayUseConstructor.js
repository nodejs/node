//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Array builtins using this['constructor'] property to construct their return values

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Array.prototype.concat",
        body: function () {
            var arr = ['a','b','c'];
            arr['constructor'] = Number;

            var out = Array.prototype.concat.call(arr, [1,2,3]);

            assert.isTrue(Array.isArray(out), "Return from Array.prototype.concat should be an Array object");
            assert.isFalse(out instanceof Number, "Return from Array.prototype.concat should not have been constructed from Number");
            assert.areEqual(['a','b','c',1,2,3], out, "Array.prototype.concat output should show correct Array behavior");
            assert.areEqual(6, out.length, "Array.prototype.concat sets the length property of returned object");
        }
    },
    {
        name: "Array.prototype.filter",
        body: function () {
            var arr = ['a','b','c'];
            arr['constructor'] = Number;

            var out = Array.prototype.filter.call(arr, function() { return true; });

            assert.isTrue(Array.isArray(out), "Return from Array.prototype.filter should be an Array object");
            assert.isFalse(out instanceof Number, "Return from Array.prototype.filter should not have been constructed from Number");
            assert.areEqual(['a','b','c'], out, "Array.prototype.filter output should show correct Array behavior");
            assert.areEqual(3, out.length, "Array.prototype.filter does not set the length property of returned object");
        }
    },
    {
        name: "Array.prototype.map",
        body: function () {
            var arr = ['a','b','c'];
            arr['constructor'] = Number;

            var out = Array.prototype.map.call(arr, function(val) { return val; });

            assert.isTrue(Array.isArray(out), "Return from Array.prototype.map should be an Array object");
            assert.isFalse(out instanceof Number, "Return from Array.prototype.map should not have been constructed from Number");
            assert.areEqual(['a','b','c'], out, "Array.prototype.map output should show correct Array behavior");
            assert.areEqual(3, out.length, "Array.prototype.map does not set the length property of returned object");
        }
    },
    {
        name: "Array.prototype.slice",
        body: function () {
            var arr = ['a','b','c'];
            arr['constructor'] = Number;

            var out = Array.prototype.slice.call(arr);

            assert.isTrue(Array.isArray(out), "Return from Array.prototype.slice should be an Array object");
            assert.isFalse(out instanceof Number, "Return from Array.prototype.slice should not have been constructed from Number");
            assert.areEqual(['a','b','c'], out, "Array.prototype.slice output should show correct Array behavior");
            assert.areEqual(3, out.length, "Array.prototype.slice sets the length property of returned object");
        }
    },
    {
        name: "Array.prototype.splice - array source with constructor property set to Number",
        body: function () {
            var arr = ['a','b','c','d','e','f'];
            arr['constructor'] = Number;

            var out = Array.prototype.splice.call(arr, 0, 3);

            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object");
            assert.isFalse(out instanceof Number, "Return from Array.prototype.splice should not have been constructed from Number");
            assert.areEqual(['a','b','c'], out, "Array.prototype.splice output should show correct Array behavior");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object");
        }
    },
    {
        name: "Array.prototype.splice - array source with constructor property set to Array",
        body: function () {
            var arr = [1,2,3,4,5,6];
            arr['constructor'] = Array;

            var out = Array.prototype.splice.call(arr, 0, 3);

            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object");
            assert.isTrue(out instanceof Array, "Return from Array.prototype.splice should have been constructed from Array");
            assert.areEqual([1,2,3], out, "Array.prototype.splice output is correct");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object");
        }
    },
    {
        name: "Array.prototype.splice - array source with no constructor property",
        body: function () {
            var arr = [1,2,3,4,5,6];

            var out = Array.prototype.splice.call(arr, 0, 3);

            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object");
            assert.isTrue(out instanceof Array, "Return from Array.prototype.splice should have been constructed from Array");
            assert.areEqual([1,2,3], out, "Array.prototype.splice output is correct");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object");
        }
    },
    {
        name: "Array.prototype.splice - object source with no constructor property",
        body: function () {
            var arr = {0:1,1:2,2:3,3:4,4:5,5:6,'length':6};

            var out = Array.prototype.splice.call(arr, 0, 3);

            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object");
            assert.isTrue(out instanceof Array, "Return from Array.prototype.splice should have been constructed from Array");
            assert.areEqual([1,2,3], out, "Array.prototype.splice output is correct");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object");
        }
    },
    {
        name: "Array.prototype.splice - object source with constructor property set to Number",
        body: function () {
            var arr = {0:1,1:2,2:3,3:4,4:5,5:6,'length':6};
            arr['constructor'] = Number;

            var out = Array.prototype.splice.call(arr, 0, 3);

            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object");
            assert.isTrue(out instanceof Array, "Return from Array.prototype.splice should have been constructed from Array");
            assert.areEqual([1,2,3], out, "Array.prototype.splice output is correct");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object");
        }
    },
    {
        name: "ArraySpeciesCreate test through Array.prototype.splice",
        body: function () {
            var arr = ['a','b','c','d','e','f'];
            arr['constructor'] = null;
            assert.throws(function() { Array.prototype.splice.call(arr, 0, 3); }, TypeError, "TypeError when constructor[Symbol.species] is not constructor", "Function 'constructor[Symbol.species]' is not a constructor");

            var arr = ['a','b','c','d','e','f'];
            Object.defineProperty(arr, 'constructor', {enumerable: false, configurable: true, writable: true, value: null});
            assert.throws(function() { Array.prototype.splice.call(arr, 0, 3); }, TypeError, "TypeError when constructor[Symbol.species] is not constructor", "Function 'constructor[Symbol.species]' is not a constructor");

            var arr = ['a','b','c','d','e','f'];
            arr['constructor'] = undefined;
            var out = Array.prototype.splice.call(arr, 0, 3);
            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object when constructor == undefined");
            assert.areEqual(['a','b','c'], out, "Array.prototype.splice output should show correct Array behavior when constructor == undefined");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object when constructor == undefined");

            var arr = ['a','b','c','d','e','f'];
            arr['constructor'] = function() {};
            var out = Array.prototype.splice.call(arr, 0, 3);
            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object when constructor has no [@@species] property");
            assert.areEqual(['a','b','c'], out, "Array.prototype.splice output should show correct Array behavior when constructor has no [@@species] property");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object when constructor has no [@@species] property");

            var builtinArraySpeciesDesc = Object.getOwnPropertyDescriptor(Array, Symbol.species);

            var arr = ['a','b','c','d','e','f'];
            Object.defineProperty(Array, Symbol.species, {enumerable: false, configurable: true, writable: true, value: Object});
            var out = Array.prototype.splice.call(arr, 0, 3);
            assert.isFalse(Array.isArray(out), "Return from Array.prototype.splice should be an object when constructor has [@@species] == Object");
            assert.areEqual({'0':'a','1':'b','2':'c',"length":3}, out, "Array.prototype.splice output should show correct Array behavior when constructor has [@@species] == Object");

            var arr = ['a','b','c','d','e','f'];
            Object.defineProperty(Array, Symbol.species, {enumerable: false, configurable: true, writable: true, value: null});
            var out = Array.prototype.splice.call(arr, 0, 3);
            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object when constructor has [@@species] == null");
            assert.areEqual(['a','b','c'], out, "Array.prototype.splice output should show correct Array behavior when constructor has [@@species] == null");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object when constructor has [@@species] == null");

            Object.defineProperty(Array, Symbol.species, builtinArraySpeciesDesc);

            var external = WScript.LoadScriptFile("ES6ArrayUseConstructor_helper.js","samethread");
            var arr = ['a','b','c','d','e','f'];
            arr['constructor'] = external.CrossContextArrayConstructor;
            var out = Array.prototype.splice.call(arr, 0, 3);
            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object when constructor is %Array% of a different script context");
            assert.areEqual(['a','b','c'], out, "Array.prototype.splice output should show correct Array behavior when constructor is %Array% of a different script context");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object when constructor is %Array% of a different script context");
        }
    },
    {
        name: "ArraySpeciesCreate test through Array.prototype.splice - native arrays",
        body: function () {
            var arr = [1,2,3,4,5,6];
            arr['constructor'] = null;
            assert.throws(function() { Array.prototype.splice.call(arr, 0, 3); }, TypeError, "TypeError when constructor[Symbol.species] is not constructor", "Function 'constructor[Symbol.species]' is not a constructor");

            var arr = [1,2,3,4,5,6];
            Object.defineProperty(arr, 'constructor', {enumerable: false, configurable: true, writable: true, value: null});
            assert.throws(function() { Array.prototype.splice.call(arr, 0, 3); }, TypeError, "TypeError when constructor[Symbol.species] is not constructor", "Function 'constructor[Symbol.species]' is not a constructor");

            var arr = [1,2,3,4,5,6];
            arr['constructor'] = undefined;
            var out = Array.prototype.splice.call(arr, 0, 3);
            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object when constructor == undefined");
            assert.areEqual([1,2,3], out, "Array.prototype.splice output should show correct Array behavior when constructor == undefined");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object when constructor == undefined");

            var arr = [1,2,3,4,5,6];
            arr['constructor'] = function() {};
            var out = Array.prototype.splice.call(arr, 0, 3);
            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object when constructor has no [@@species] property");
            assert.areEqual([1,2,3], out, "Array.prototype.splice output should show correct Array behavior when constructor has no [@@species] property");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object when constructor has no [@@species] property");

            var builtinArraySpeciesDesc = Object.getOwnPropertyDescriptor(Array, Symbol.species);

            var arr = [1,2,3,4,5,6];
            Object.defineProperty(Array, Symbol.species, {enumerable: false, configurable: true, writable: true, value: Object});
            var out = Array.prototype.splice.call(arr, 0, 3);
            assert.isFalse(Array.isArray(out), "Return from Array.prototype.splice should be an object when constructor has [@@species] == Object");
            assert.areEqual({'0':1,'1':2,'2':3,"length":3}, out, "Array.prototype.splice output should show correct Array behavior when constructor has [@@species] == Object");

            var arr = [1,2,3,4,5,6];
            Object.defineProperty(Array, Symbol.species, {enumerable: false, configurable: true, writable: true, value: null});
            var out = Array.prototype.splice.call(arr, 0, 3);
            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object when constructor has [@@species] == null");
            assert.areEqual([1,2,3], out, "Array.prototype.splice output should show correct Array behavior when constructor has [@@species] == null");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object when constructor has [@@species] == null");

            Object.defineProperty(Array, Symbol.species, builtinArraySpeciesDesc);

            var external = WScript.LoadScriptFile("ES6ArrayUseConstructor_helper.js","samethread");
            var arr = [1,2,3,4,5,6];
            arr['constructor'] = external.CrossContextArrayConstructor;
            var out = Array.prototype.splice.call(arr, 0, 3);
            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object when constructor is %Array% of a different script context");
            assert.areEqual([1,2,3], out, "Array.prototype.splice output should show correct Array behavior when constructor is %Array% of a different script context");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object when constructor is %Array% of a different script context");
        }
    },
    {
        name: "ArraySpeciesCreate test through Array.prototype.map",
        body: function () {
            var f = function(val) { return val; }
            var arr = ['a','b','c'];
            arr['constructor'] = null;
            assert.throws(function() { Array.prototype.map.call(arr, f); }, TypeError, "TypeError when constructor[Symbol.species] is not constructor", "Function 'constructor[Symbol.species]' is not a constructor");

            var arr = ['a','b','c'];
            Object.defineProperty(arr, 'constructor', {enumerable: false, configurable: true, writable: true, value: null});
            assert.throws(function() { Array.prototype.map.call(arr, f); }, TypeError, "TypeError when constructor[Symbol.species] is not constructor", "Function 'constructor[Symbol.species]' is not a constructor");

            var arr = ['a','b','c'];
            arr['constructor'] = undefined;
            var out = Array.prototype.map.call(arr, f);
            assert.isTrue(Array.isArray(out), "Return from Array.prototype.map should be an Array object when constructor == undefined");
            assert.areEqual(['a','b','c'], out, "Array.prototype.map output should show correct Array behavior when constructor == undefined");
            assert.areEqual(3, out.length, "Array.prototype.map sets the length property of returned object when constructor == undefined");

            var arr = ['a','b','c'];
            arr['constructor'] = function() {};
            var out = Array.prototype.map.call(arr, f);
            assert.isTrue(Array.isArray(out), "Return from Array.prototype.map should be an Array object when constructor has no [@@species] property");
            assert.areEqual(['a','b','c'], out, "Array.prototype.map output should show correct Array behavior when constructor has no [@@species] property");
            assert.areEqual(3, out.length, "Array.prototype.map sets the length property of returned object when constructor has no [@@species] property");

            var builtinArraySpeciesDesc = Object.getOwnPropertyDescriptor(Array, Symbol.species);

            var arr = ['a','b','c'];
            Object.defineProperty(Array, Symbol.species, {enumerable: false, configurable: true, writable: true, value: Object});
            var out = Array.prototype.map.call(arr, f);
            assert.isFalse(Array.isArray(out), "Return from Array.prototype.map should be an object when constructor has [@@species] == Object");
            assert.areEqual({'0':'a','1':'b','2':'c'}, out, "Array.prototype.map output should show correct Array behavior when constructor has [@@species] == Object");

            var arr = ['a','b','c'];
            Object.defineProperty(Array, Symbol.species, {enumerable: false, configurable: true, writable: true, value: null});
            var out = Array.prototype.map.call(arr, f);
            assert.isTrue(Array.isArray(out), "Return from Array.prototype.map should be an Array object when constructor has [@@species] == null");
            assert.areEqual(['a','b','c'], out, "Array.prototype.map output should show correct Array behavior when constructor has [@@species] == null");
            assert.areEqual(3, out.length, "Array.prototype.map sets the length property of returned object when constructor has [@@species] == null");

            Object.defineProperty(Array, Symbol.species, builtinArraySpeciesDesc);

            var external = WScript.LoadScriptFile("ES6ArrayUseConstructor_helper.js","samethread");
            var arr = ['a','b','c'];
            arr['constructor'] = external.CrossContextArrayConstructor;
            var out = Array.prototype.map.call(arr, f);
            assert.isTrue(Array.isArray(out), "Return from Array.prototype.map should be an Array object when constructor is %Array% of a different script context");
            assert.areEqual(['a','b','c'], out, "Array.prototype.map output should show correct Array behavior when constructor is %Array% of a different script context");
            assert.areEqual(3, out.length, "Array.prototype.map sets the length property of returned object when constructor is %Array% of a different script context");
        }
    },
    {
        name: "ArraySpeciesCreate test through Array.prototype.map - native arrays",
        body: function () {
            var f = function(val) { return val; }
            var arr = [1,2,3];
            arr['constructor'] = null;
            assert.throws(function() { Array.prototype.map.call(arr, f); }, TypeError, "TypeError when constructor[Symbol.species] is not constructor", "Function 'constructor[Symbol.species]' is not a constructor");

            var arr = [1,2,3];
            Object.defineProperty(arr, 'constructor', {enumerable: false, configurable: true, writable: true, value: null});
            assert.throws(function() { Array.prototype.map.call(arr, f); }, TypeError, "TypeError when constructor[Symbol.species] is not constructor", "Function 'constructor[Symbol.species]' is not a constructor");

            var arr = [1,2,3];
            arr['constructor'] = undefined;
            var out = Array.prototype.map.call(arr, f);
            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object when constructor == undefined");
            assert.areEqual([1,2,3], out, "Array.prototype.splice output should show correct Array behavior when constructor == undefined");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object when constructor == undefined");

            var arr = [1,2,3];
            arr['constructor'] = function() {};
            var out = Array.prototype.map.call(arr, f);
            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object when constructor has no [@@species] property");
            assert.areEqual([1,2,3], out, "Array.prototype.splice output should show correct Array behavior when constructor has no [@@species] property");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object when constructor has no [@@species] property");

            var builtinArraySpeciesDesc = Object.getOwnPropertyDescriptor(Array, Symbol.species);

            var arr = [1,2,3];
            Object.defineProperty(Array, Symbol.species, {enumerable: false, configurable: true, writable: true, value: Object});
            var out = Array.prototype.map.call(arr, f);
            assert.isFalse(Array.isArray(out), "Return from Array.prototype.splice should be an object when constructor has [@@species] == Object");
            assert.areEqual({'0':1,'1':2,'2':3}, out, "Array.prototype.splice output should show correct Array behavior when constructor has [@@species] == Object");

            var arr = [1,2,3];
            Object.defineProperty(Array, Symbol.species, {enumerable: false, configurable: true, writable: true, value: null});
            var out = Array.prototype.map.call(arr, f);
            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object when constructor has [@@species] == null");
            assert.areEqual([1,2,3], out, "Array.prototype.splice output should show correct Array behavior when constructor has [@@species] == null");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object when constructor has [@@species] == null");

            Object.defineProperty(Array, Symbol.species, builtinArraySpeciesDesc);

            var external = WScript.LoadScriptFile("ES6ArrayUseConstructor_helper.js","samethread");
            var arr = [1,2,3];
            arr['constructor'] = external.CrossContextArrayConstructor;
            var out = Array.prototype.map.call(arr, f);
            assert.isTrue(Array.isArray(out), "Return from Array.prototype.splice should be an Array object when constructor is %Array% of a different script context");
            assert.areEqual([1,2,3], out, "Array.prototype.splice output should show correct Array behavior when constructor is %Array% of a different script context");
            assert.areEqual(3, out.length, "Array.prototype.splice sets the length property of returned object when constructor is %Array% of a different script context");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
