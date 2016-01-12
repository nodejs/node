//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Basic WeakMap tests -- verifies the API shape

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "WeakMap is a constructor on the global object",
        body: function () {
            assert.isTrue(WeakMap !== undefined, "WeakMap should be defined");

            var wm1 = new WeakMap();
            // WeakMap is no longer allowed to be called as a function unless the object it is given
            // for its this argument already has the [[WeakMapData]] property on it.
            // var wm2 = WeakMap();

            assert.isTrue(wm1 instanceof WeakMap, "'new WeakMap()' should create a WeakMap object");
            //assert.isTrue(wm2 instanceof WeakMap, "'WeakMap()' should also create a WeakMap object");
            //assert.isTrue(wm1 !== wm2, "Should be two different WeakMap objects");

            assert.areEqual(0, WeakMap.length, "WeakMap takes one optional argument and spec says length must be 0");
        }
    },
    {
        name: "WeakMap.prototype should have spec defined built-ins",
        body: function () {
            assert.isTrue(WeakMap.prototype.constructor === WeakMap, "WeakMap.prototype should have a constructor property set to WeakMap");

            assert.isTrue(WeakMap.prototype.hasOwnProperty('delete'), "WeakMap.prototype should have a delete method");
            assert.isTrue(WeakMap.prototype.hasOwnProperty('get'), "WeakMap.prototype should have a get method");
            assert.isTrue(WeakMap.prototype.hasOwnProperty('has'), "WeakMap.prototype should have a has method");
            assert.isTrue(WeakMap.prototype.hasOwnProperty('set'), "WeakMap.prototype should have a set method");

            assert.isTrue(WeakMap.prototype.delete.length === 1, "delete method takes one argument");
            assert.isTrue(WeakMap.prototype.get.length === 1, "get method takes one argument");
            assert.isTrue(WeakMap.prototype.has.length === 1, "has method takes one argument");
            assert.isTrue(WeakMap.prototype.set.length === 2, "set method takes two arguments");
        }
    },
    {
        name: "WeakMap objects' prototype should be WeakMap.prototype",
        body: function() {
            var wm1 = new WeakMap();
            // WeakMap is no longer allowed to be called as a function unless the object it is given
            // for its this argument already has the [[WeakMapData]] property on it.
            // var wm2 = WeakMap();

            assert.isTrue(Object.getPrototypeOf(wm1) === WeakMap.prototype, "'new WeakMap()' should set the prototype of the returned object to WeakMap.prototype");
            //assert.isTrue(Object.getPrototypeOf(wm2) === WeakMap.prototype, "'WeakMap()' should set the prototype of the returned object to WeakMap.prototype");
        }
    },
    {
        name: "toString of a WeakMap object returns [object WeakMap]",
        body: function () {
            var wm = new WeakMap();

            assert.areEqual("[object WeakMap]", '' + wm, "toString() of map returns [object WeakMap]");
        }
    },
    {
        name: "WeakMap objects are normal extensible dynamic objects",
        body: function () {
            function countEnumerableProperties(o) {
                var count = 0;
                for (p in o) {
                    count += 1;
                }
                return count;
            }

            var wm = new WeakMap();

            assert.isTrue(countEnumerableProperties(WeakMap.prototype) == 0, "Built-in methods should not be enumerable on the prototype object");
            assert.isTrue(countEnumerableProperties(wm) == 0, "Built-in methods should not be enumerable on an instance object");

            wm.foo = 10;
            wm.bar = 'hello';

            assert.isTrue(countEnumerableProperties(wm) == 2, "Should be able to add user properties");
            assert.isTrue(wm.foo === 10, "Property value should be set and retrieved correctly");
            assert.isTrue(wm.bar === 'hello', "Property value should be set and retrieved correctly");

            delete wm.foo;
            assert.isTrue(countEnumerableProperties(wm) == 1, "Should be able to delete user properties");
            assert.isTrue(wm.foo === undefined, "Should be able to delete user properties");
        }
    },
    {
        name: "WeakMap is subclassable",
        body: function () {
            // WeakMap is no longer allowed to be called as a function unless the object it is given
            // for its this argument already has the [[WeakMapData]] property on it.
            //
            // For IE11 we simply throw if WeakMap() is called as a function instead of in a new expression
            assert.throws(function () { WeakMap.call(); }, TypeError, "WeakMap.call() throws TypeError");
            assert.throws(function () { WeakMap.call({ }); }, TypeError, "WeakMap.call() throws TypeError given an object");
            assert.throws(function () { WeakMap.call(123); }, TypeError, "WeakMap.call() throws TypeError given a number");
            assert.throws(function () { WeakMap.call("hello"); }, TypeError, "WeakMap.call() throws TypeError given a string");

            function MyWeakMap() {
                WeakMap.call(this);
            }
            MyWeakMap.prototype = new WeakMap();
            MyWeakMap.prototype.constructor = MyWeakMap;

            assert.throws(function () { var mymap = new MyWeakMap(); }, TypeError, "WeakMap.call(this) throws TypeError when used in the old subclassing pattern");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
