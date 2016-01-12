//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Basic WeakSet tests -- verifies the API shape

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "WeakSet is a constructor on the global object",
        body: function () {
            assert.isTrue(WeakSet !== undefined, "WeakSet should be defined");

            var ws1 = new WeakSet();
            // WeakSet is no longer allowed to be called as a function unless the object it is given
            // for its this argument already has the [[WeakSetData]] property on it.
            // TODO: When we implement @@create support, update this test to reflect it.
            //var ws2 = WeakSet();

            assert.isTrue(ws1 instanceof WeakSet, "'new WeakSet()' should create a WeakSet object");
            //assert.isTrue(ws2 instanceof WeakSet, "'WeakSet()' should also create a WeakSet object");
            //assert.isTrue(ws1 !== ws2, "Should be two different WeakSet objects");

            assert.areEqual(0, WeakSet.length, "WeakSet takes one optional argument and spec says length must be 0");
        }
    },
    {
        name: "WeakSet.prototype should have spec defined built-ins",
        body: function () {
            assert.isTrue(WeakSet.prototype.constructor === WeakSet, "WeakSet.prototype should have a constructor property set to WeakSet");

            assert.isTrue(WeakSet.prototype.hasOwnProperty('add'), "WeakSet.prototype should have a add method");
            assert.isTrue(WeakSet.prototype.hasOwnProperty('delete'), "WeakSet.prototype should have a delete method");
            assert.isTrue(WeakSet.prototype.hasOwnProperty('has'), "WeakSet.prototype should have a has method");

            assert.isTrue(WeakSet.prototype.add.length === 1, "add method takes two arguments");
            assert.isTrue(WeakSet.prototype.delete.length === 1, "delete method takes one argument");
            assert.isTrue(WeakSet.prototype.has.length === 1, "has method takes one argument");
        }
    },
    {
        name: "WeakSet objects' prototype should be WeakSet.prototype",
        body: function() {
            var ws1 = new WeakSet();
            // WeakSet is no longer allowed to be called as a function unless the object it is given
            // for its this argument already has the [[WeakSetData]] property on it.
            // TODO: When we implement @@create support, update this test to reflect it.
            //var ws2 = WeakSet();

            assert.isTrue(Object.getPrototypeOf(ws1) === WeakSet.prototype, "'new WeakSet()' should set the prototype of the returned object to WeakSet.prototype");
            //assert.isTrue(Object.getPrototypeOf(ws2) === WeakSet.prototype, "'WeakSet()' should set the prototype of the returned object to WeakSet.prototype");
        }
    },
    {
        name: "toString of a WeakSet object returns [object WeakSet]",
        body: function () {
            var ws = new WeakSet();

            assert.areEqual("[object WeakSet]", '' + ws, "toString() of map returns [object WeakSet]");
        }
    },
    {
        name: "WeakSet objects are normal extensible dynamic objects",
        body: function () {
            function countEnumerableProperties(o) {
                var count = 0;
                for (p in o) {
                    count += 1;
                }
                return count;
            }

            var ws = new WeakSet();

            assert.isTrue(countEnumerableProperties(WeakSet.prototype) == 0, "Built-in methods should not be enumerable on the prototype object");
            assert.isTrue(countEnumerableProperties(ws) == 0, "Built-in methods should not be enumerable on an instance object");

            ws.foo = 10;
            ws.bar = 'hello';

            assert.isTrue(countEnumerableProperties(ws) == 2, "Should be able to add user properties");
            assert.isTrue(ws.foo === 10, "Property value should be set and retrieved correctly");
            assert.isTrue(ws.bar === 'hello', "Property value should be set and retrieved correctly");

            delete ws.foo;
            assert.isTrue(countEnumerableProperties(ws) == 1, "Should be able to delete user properties");
            assert.isTrue(ws.foo === undefined, "Should be able to delete user properties");
        }
    },
    {
        name: "WeakSet is subclassable",
        body: function () {
            // WeakSet is no longer allowed to be called as a function unless the object it is given
            // for its this argument already has the [[WeakSetData]] property on it.
            // TODO: When we implement @@create support, update this test to reflect it.
            //
            // For IE11 we simply throw if WeakSet() is called as a function instead of in a new expression
            assert.throws(function () { WeakSet.call(); }, TypeError, "WeakSet.call() throws TypeError");
            assert.throws(function () { WeakSet.call({ }); }, TypeError, "WeakSet.call() throws TypeError given an object");
            assert.throws(function () { WeakSet.call(123); }, TypeError, "WeakSet.call() throws TypeError given a number");
            assert.throws(function () { WeakSet.call("hello"); }, TypeError, "WeakSet.call() throws TypeError given a string");

            function MyWeakSet() {
                WeakSet.call(this);
            }
            MyWeakSet.prototype = new WeakSet();
            MyWeakSet.prototype.constructor = MyWeakSet;

            assert.throws(function () { var mymap = new MyWeakSet(); }, TypeError, "WeakSet.call(this) throws TypeError when used in the old subclassing pattern");
            /*
            function MyWeakSet() {
                WeakSet.call(this);
            }
            MyWeakSet.prototype = new WeakSet();
            MyWeakSet.prototype.constructor = MyWeakSet();

            var myWeakSet = new MyWeakSet();

            assert.isTrue(myWeakSet instanceof MyWeakSet, "Should be a MyWeakSet object");
            assert.isTrue(myWeakSet instanceof WeakSet, "Should also be a WeakSet object");
            assert.isTrue(Object.getPrototypeOf(myWeakSet) === MyWeakSet.prototype, "Should have MyWeakSet prototype");
            assert.isTrue(Object.getPrototypeOf(myWeakSet) !== WeakSet.prototype, "Should be distinct from WeakSet prototype");

            assert.isTrue(myWeakSet.delete instanceof Function, "Should have a delete method");
            assert.isTrue(myWeakSet.get instanceof Function, "Should have a get method");
            assert.isTrue(myWeakSet.has instanceof Function, "Should have a has method");
            assert.isTrue(myWeakSet.set instanceof Function, "Should have a set method");
            */
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
