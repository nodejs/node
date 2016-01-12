//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Basic Map tests -- verifies the API shape

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Map is a constructor on the global object",
        body: function () {
            assert.isTrue(Map !== undefined, "Map should be defined");

            var map1 = new Map();
            // Map is no longer allowed to be called as a function unless the object it is given
            // for its this argument already has the [[MapData]] property on it.
            // TODO: When we implement @@create support, update this test to reflect it.
            //var map2 = Map();

            assert.isTrue(map1 instanceof Map, "'new Map()' should create a Map object");
            //assert.isTrue(map2 instanceof Map, "'Map()' should also create a Map object");
            //assert.isTrue(map1 !== map2, "Should be two different Map objects");

            assert.areEqual(0, Map.length, "Map takes one optional argument and spec says length must be 0");
        }
    },
    {
        name: "Map.prototype should have spec defined built-ins",
        body: function () {
            assert.isTrue(Map.prototype.constructor === Map, "Map.prototype should have a constructor property set to Map");

            assert.isTrue(Map.prototype.hasOwnProperty('clear'), "Map.prototype should have a clear method");
            assert.isTrue(Map.prototype.hasOwnProperty('delete'), "Map.prototype should have a delete method");
            assert.isTrue(Map.prototype.hasOwnProperty('forEach'), "Map.prototype should have a forEach method");
            assert.isTrue(Map.prototype.hasOwnProperty('get'), "Map.prototype should have a get method");
            assert.isTrue(Map.prototype.hasOwnProperty('has'), "Map.prototype should have a has method");
            assert.isTrue(Map.prototype.hasOwnProperty('set'), "Map.prototype should have a set method");
            assert.isTrue(Map.prototype.hasOwnProperty('size'), "Map.prototype should have a size accessor");

            assert.isTrue(Map.prototype.clear.length === 0, "clear method takes zero arguments");
            assert.isTrue(Map.prototype.delete.length === 1, "delete method takes one argument");
            assert.isTrue(Map.prototype.forEach.length === 1, "forEach method takes two arguments but second is optional and spec says length must be 1");
            assert.isTrue(Map.prototype.get.length === 1, "get method takes one argument");
            assert.isTrue(Map.prototype.has.length === 1, "has method takes one argument");
            assert.isTrue(Map.prototype.set.length === 2, "set method takes two arguments");

            assert.isTrue(Object.getOwnPropertyDescriptor(Map.prototype, "size").get !== undefined, "size accessor should have get method");
            assert.isTrue(Object.getOwnPropertyDescriptor(Map.prototype, "size").set === undefined, "size accessor should not have set method");
        }
    },
    {
        name: "Map objects' prototype should be Map.prototype",
        body: function () {
            var map1 = new Map();
            // Map is no longer allowed to be called as a function unless the object it is given
            // for its this argument already has the [[MapData]] property on it.
            // TODO: When we implement @@create support, update this test to reflect it.
            //var map2 = Map();

            assert.isTrue(Object.getPrototypeOf(map1) === Map.prototype, "'new Map()' should set the prototype of the returned object to Map.prototype");
            //assert.isTrue(Object.getPrototypeOf(map2) === Map.prototype, "'Map()' should set the prototype of the returned object to Map.prototype");
        }
    },
    {
        name: "toString of a Map object returns [object Map]",
        body: function () {
            var map = new Map();

            assert.areEqual("[object Map]", '' + map, "toString() of map returns [object Map]");
        }
    },
    {
        name: "Map objects are normal extensible dynamic objects",
        body: function () {
            function countEnumerableProperties(o) {
                var count = 0;
                for (p in o) {
                    count += 1;
                }
                return count;
            }

            var map = new Map();

            assert.isTrue(countEnumerableProperties(Map.prototype) == 0, "Built-in methods should not be enumerable on the prototype object");
            assert.isTrue(countEnumerableProperties(map) == 0, "Built-in methods should not be enumerable on an instance object");

            map.foo = 10;
            map.bar = 'hello';

            assert.isTrue(countEnumerableProperties(map) == 2, "Should be able to add user properties");
            assert.isTrue(map.foo === 10, "Property value should be set and retrieved correctly");
            assert.isTrue(map.bar === 'hello', "Property value should be set and retrieved correctly");

            delete map.foo;
            assert.isTrue(countEnumerableProperties(map) == 1, "Should be able to delete user properties");
            assert.isTrue(map.foo === undefined, "Should be able to delete user properties");
        }
    },
    {
        name: "Map is subclassable",
        body: function () {
            // Map is no longer allowed to be called as a function unless the object it is given
            // for its this argument already has the [[MapData]] property on it.
            // TODO: When we implement @@create support, update this test to reflect it.
            //
            // For IE11 we simply throw if Map() is called as a function instead of in a new expression
            assert.throws(function () { Map.call(); }, TypeError, "Map.call() throws TypeError");
            assert.throws(function () { Map.call({ }); }, TypeError, "Map.call() throws TypeError given an object");
            assert.throws(function () { Map.call(123); }, TypeError, "Map.call() throws TypeError given a number");
            assert.throws(function () { Map.call("hello"); }, TypeError, "Map.call() throws TypeError given a string");

            function MyMap() {
                Map.call(this);
            }
            MyMap.prototype = new Map();
            MyMap.prototype.constructor = MyMap;

            assert.throws(function () { var mymap = new MyMap(); }, TypeError, "Map.call(this) throws TypeError when used in the old subclassing pattern");
            /*
            function MyMap() {
                Map.call(this);
            }
            MyMap.prototype = new Map();
            MyMap.prototype.constructor = MyMap;

            var mymap = new MyMap();

            assert.isTrue(mymap instanceof MyMap, "Should be a MyMap object");
            assert.isTrue(mymap instanceof Map, "Should also be a Map object");
            assert.isTrue(Object.getPrototypeOf(mymap) === MyMap.prototype, "Should have MyMap prototype");
            assert.isTrue(Object.getPrototypeOf(mymap) !== Map.prototype, "Should be distinct from Map prototype");

            assert.isTrue(mymap.clear instanceof Function, "Should have clear method");
            assert.isTrue(mymap.delete instanceof Function, "Should have delete method");
            assert.isTrue(mymap.forEach instanceof Function, "Should have forEach method");
            assert.isTrue(mymap.get instanceof Function, "Should have get method");
            assert.isTrue(mymap.has instanceof Function, "Should have has method");
            assert.isTrue(mymap.set instanceof Function, "Should have set method");
            assert.isTrue(mymap.size === 0, "Should have size getter (initially zero)");
            */
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
