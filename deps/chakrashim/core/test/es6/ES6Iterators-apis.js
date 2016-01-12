//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Iterators Built-In APIs tests -- verifies the shape and basic behavior of the built-in iterators (Array, String, Map, Set)

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function getNewMapWith12345() {
    var map = new Map();

    map.set(1, 6);
    map.set(2, 7);
    map.set(3, 8);
    map.set(4, 9);
    map.set(5, 10);

    return map;
}

function getNewSetWith12345() {
    var set = new Set();
    set.add(1);
    set.add(2);
    set.add(3);
    set.add(4);
    set.add(5);

    return set;
}

var tests = [
    {
        name: "%IteratorPrototype% apis",
        body: function () {
            var iteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]()));

            assert.isTrue(iteratorPrototype.hasOwnProperty(Symbol.iterator), "%IteratorPrototype% should have a @@iterator method");

            assert.areEqual(0, iteratorPrototype[Symbol.iterator].length, "@@iterator method takes zero arguments");
        }
    },
    {
        name: "%IteratorPrototype% is the prototype of all built-in iterators",
        body: function () {
            var arrayIteratorPrototype = Object.getPrototypeOf([][Symbol.iterator]());
            var mapIteratorPrototype = Object.getPrototypeOf((new Map())[Symbol.iterator]());
            var setIteratorPrototype = Object.getPrototypeOf((new Set())[Symbol.iterator]());
            var stringIteratorPrototype = Object.getPrototypeOf(""[Symbol.iterator]());

            // The only way to get the $IteratorPrototype% object is indirectly so here
            // we just assume array is correct and get it from an array iterator.
            var iteratorPrototype = Object.getPrototypeOf(arrayIteratorPrototype);

            assert.areEqual(iteratorPrototype, Object.getPrototypeOf(mapIteratorPrototype), "%MapIteratorPrototype%'s prototype is %IteratorPrototype%");
            assert.areEqual(iteratorPrototype, Object.getPrototypeOf(setIteratorPrototype), "%SetIteratorPrototype%'s prototype is %IteratorPrototype%");
            assert.areEqual(iteratorPrototype, Object.getPrototypeOf(stringIteratorPrototype), "%StringIteratorPrototype%'s prototype is %IteratorPrototype%");
        }
    },
    {
        name: "Array.prototype should have iterator APIs (entries, keys, values, @@iterator)",
        body: function () {
            assert.isTrue(Array.prototype.hasOwnProperty('entries'), "Array.prototype should have an entries method");
            assert.isTrue(Array.prototype.hasOwnProperty('keys'), "Array.prototype should have a keys method");
            assert.isTrue(Array.prototype.hasOwnProperty('values'), "Array.prototype should have a values method");
            assert.isTrue(Array.prototype.hasOwnProperty(Symbol.iterator), "Array.prototype should have an @@iterator method");

            assert.areEqual(0, Array.prototype.entries.length, "entries method takes zero arguments");
            assert.areEqual(0, Array.prototype.keys.length, "keys method takes zero arguments");
            assert.areEqual(0, Array.prototype.values.length, "values method takes zero arguments");

            assert.isTrue(Array.prototype.values === Array.prototype[Symbol.iterator], "Array.prototype's @@iterator is the same function as its values() method");
        }
    },
    {
        name: "Map.prototype should have iterator APIs (entries, keys, values, @@iterator)",
        body: function () {
            assert.isTrue(Map.prototype.hasOwnProperty('entries'), "Map.prototype should have an entries method");
            assert.isTrue(Map.prototype.hasOwnProperty('keys'), "Map.prototype should have a keys method");
            assert.isTrue(Map.prototype.hasOwnProperty('values'), "Map.prototype should have a values method");
            assert.isTrue(Map.prototype.hasOwnProperty(Symbol.iterator), "Map.prototype should have an @@iterator method");

            assert.areEqual(0, Map.prototype.entries.length, "entries method takes zero arguments");
            assert.areEqual(0, Map.prototype.keys.length, "keys method takes zero arguments");
            assert.areEqual(0, Map.prototype.values.length, "values method takes zero arguments");

            assert.isTrue(Map.prototype.entries === Map.prototype[Symbol.iterator], "Map.prototype's @@iterator is the same function as its entries() method");
        }
    },
    {
        name: "Set.prototype should have iterator APIs (entries, keys, values, @@iterator)",
        body: function () {
            assert.isTrue(Set.prototype.hasOwnProperty('entries'), "Set.prototype should have an entries method");
            assert.isTrue(Set.prototype.hasOwnProperty('keys'), "Set.prototype should have a keys method");
            assert.isTrue(Set.prototype.hasOwnProperty('values'), "Set.prototype should have a values method");
            assert.isTrue(Set.prototype.hasOwnProperty(Symbol.iterator), "Set.prototype should have an @@iterator method");

            assert.areEqual(0, Set.prototype.entries.length, "entries method takes zero arguments");
            assert.areEqual(0, Set.prototype.values.length, "values method takes zero arguments");

            assert.isTrue(Set.prototype.values === Set.prototype.keys, "Set.prototype's keys property is the same function as its values() method");
            assert.isTrue(Set.prototype.values === Set.prototype[Symbol.iterator], "Set.prototype's @@iterator is the same function as its values() method");
        }
    },
    {
        name: "String.prototype should have iterator APIs (@@iterator only)",
        body: function () {
            assert.isTrue(String.prototype.hasOwnProperty(Symbol.iterator), "String.prototype should have an @@iterator method");

            assert.isFalse(String.prototype.hasOwnProperty('entries'), "String.prototype should not have an entries method");
            assert.isFalse(String.prototype.hasOwnProperty('keys'), "String.prototype should not have a keys method");
            assert.isFalse(String.prototype.hasOwnProperty('values'), "String.prototype should not have a values method");
        }
    },
    {
        name: "Array.prototype iterator APIs should throw when called with this equal to undefined or null",
        body: function () {
            assert.throws(function () { Array.prototype.entries.call(null); }, TypeError, "Array.protoype.entries throws if this is null", "Array.prototype.entries: 'this' is null or undefined");
            assert.throws(function () { Array.prototype.entries.call(undefined); }, TypeError, "Array.protoype.entries throws if this is undefined", "Array.prototype.entries: 'this' is null or undefined");

            assert.throws(function () { Array.prototype.keys.call(null); }, TypeError, "Array.protoype.keys throws if this is null", "Array.prototype.keys: 'this' is null or undefined");
            assert.throws(function () { Array.prototype.keys.call(undefined); }, TypeError, "Array.protoype.keys throws if this is undefined", "Array.prototype.keys: 'this' is null or undefined");

            assert.throws(function () { Array.prototype.values.call(null); }, TypeError, "Array.protoype.values throws if this is null", "Array.prototype.values: 'this' is null or undefined");
            assert.throws(function () { Array.prototype.values.call(undefined); }, TypeError, "Array.protoype.values throws if this is undefined", "Array.prototype.values: 'this' is null or undefined");
        }
    },
    {
        name: "Map.prototype iterator APIs should throw when called with this equal to anything other than a Map object",
        body: function () {
            assert.throws(function () { Map.prototype.entries.call(null); }, TypeError, "Map.protoype.entries throws if this is null", "Map.prototype.entries: 'this' is not a Map object");
            assert.throws(function () { Map.prototype.entries.call(undefined); }, TypeError, "Map.protoype.entries throws if this is undefined", "Map.prototype.entries: 'this' is not a Map object");
            assert.throws(function () { Map.prototype.entries.call(123); }, TypeError, "Map.protoype.entries throws if this is a number", "Map.prototype.entries: 'this' is not a Map object");
            assert.throws(function () { Map.prototype.entries.call("abc"); }, TypeError, "Map.protoype.entries throws if this is a string", "Map.prototype.entries: 'this' is not a Map object");
            assert.throws(function () { Map.prototype.entries.call({ }); }, TypeError, "Map.protoype.entries throws if this is a non-Map object", "Map.prototype.entries: 'this' is not a Map object");
            assert.throws(function () { Map.prototype.entries.call(new Set()); }, TypeError, "Map.protoype.entries throws if this is a non-Map object (e.g. a Set)", "Map.prototype.entries: 'this' is not a Map object");

            assert.throws(function () { Map.prototype.keys.call(null); }, TypeError, "Map.protoype.keys throws if this is null", "Map.prototype.keys: 'this' is not a Map object");
            assert.throws(function () { Map.prototype.keys.call(undefined); }, TypeError, "Map.protoype.keys throws if this is undefined", "Map.prototype.keys: 'this' is not a Map object");
            assert.throws(function () { Map.prototype.keys.call(123); }, TypeError, "Map.protoype.keys throws if this is a number", "Map.prototype.keys: 'this' is not a Map object");
            assert.throws(function () { Map.prototype.keys.call("abc"); }, TypeError, "Map.protoype.keys throws if this is a string", "Map.prototype.keys: 'this' is not a Map object");
            assert.throws(function () { Map.prototype.keys.call({ }); }, TypeError, "Map.protoype.keys throws if this is a non-Map object", "Map.prototype.keys: 'this' is not a Map object");
            assert.throws(function () { Map.prototype.keys.call(new Set()); }, TypeError, "Map.protoype.keys throws if this is a non-Map object (e.g. a Set)", "Map.prototype.keys: 'this' is not a Map object");

            assert.throws(function () { Map.prototype.values.call(null); }, TypeError, "Map.protoype.values throws if this is null", "Map.prototype.values: 'this' is not a Map object");
            assert.throws(function () { Map.prototype.values.call(undefined); }, TypeError, "Map.protoype.values throws if this is undefined", "Map.prototype.values: 'this' is not a Map object");
            assert.throws(function () { Map.prototype.values.call(123); }, TypeError, "Map.protoype.values throws if this is a number", "Map.prototype.values: 'this' is not a Map object");
            assert.throws(function () { Map.prototype.values.call("abc"); }, TypeError, "Map.protoype.values throws if this is a string", "Map.prototype.values: 'this' is not a Map object");
            assert.throws(function () { Map.prototype.values.call({ }); }, TypeError, "Map.protoype.values throws if this is a non-Map object", "Map.prototype.values: 'this' is not a Map object");
            assert.throws(function () { Map.prototype.values.call(new Set()); }, TypeError, "Map.protoype.values throws if this is a non-Map object (e.g. a Set)", "Map.prototype.values: 'this' is not a Map object");
        }
    },
    {
        name: "Set.prototype iterator APIs should throw when called with this equal to anything other than a Set object",
        body: function () {
            assert.throws(function () { Set.prototype.entries.call(null); }, TypeError, "Set.protoype.entries throws if this is null", "Set.prototype.entries: 'this' is not a Set object");
            assert.throws(function () { Set.prototype.entries.call(undefined); }, TypeError, "Set.protoype.entries throws if this is undefined", "Set.prototype.entries: 'this' is not a Set object");
            assert.throws(function () { Set.prototype.entries.call(123); }, TypeError, "Set.protoype.entries throws if this is a number", "Set.prototype.entries: 'this' is not a Set object");
            assert.throws(function () { Set.prototype.entries.call("abc"); }, TypeError, "Set.protoype.entries throws if this is a string", "Set.prototype.entries: 'this' is not a Set object");
            assert.throws(function () { Set.prototype.entries.call({ }); }, TypeError, "Set.protoype.entries throws if this is a non-Set object", "Set.prototype.entries: 'this' is not a Set object");
            assert.throws(function () { Set.prototype.entries.call(new Map()); }, TypeError, "Set.protoype.entries throws if this is a non-Set object (e.g. a Map)", "Set.prototype.entries: 'this' is not a Set object");

            assert.throws(function () { Set.prototype.values.call(null); }, TypeError, "Set.protoype.values throws if this is null", "Set.prototype.values: 'this' is not a Set object");
            assert.throws(function () { Set.prototype.values.call(undefined); }, TypeError, "Set.protoype.values throws if this is undefined", "Set.prototype.values: 'this' is not a Set object");
            assert.throws(function () { Set.prototype.values.call(123); }, TypeError, "Set.protoype.values throws if this is a number", "Set.prototype.values: 'this' is not a Set object");
            assert.throws(function () { Set.prototype.values.call("abc"); }, TypeError, "Set.protoype.values throws if this is a string", "Set.prototype.values: 'this' is not a Set object");
            assert.throws(function () { Set.prototype.values.call({ }); }, TypeError, "Set.protoype.values throws if this is a non-Set object", "Set.prototype.values: 'this' is not a Set object");
            assert.throws(function () { Set.prototype.values.call(new Map()); }, TypeError, "Set.protoype.values throws if this is a non-Set object (e.g. a Map)", "Set.prototype.values: 'this' is not a Set object");
        }
    },
    {
        name: "String.prototype iterator APIs should throw when called with this equal to undefined or null",
        body: function () {
            assert.throws(function () { String.prototype[Symbol.iterator].call(null); }, TypeError, "String.protoype[Symbol.iterator] throws if this is null", "String.prototype[Symbol.iterator]: 'this' is null or undefined");
            assert.throws(function () { String.prototype[Symbol.iterator].call(undefined); }, TypeError, "String.protoype[Symbol.iterator] throws if this is undefined", "String.prototype[Symbol.iterator]: 'this' is null or undefined");
        }
    },
    {
        name: "%ArrayIteratorPrototype% should have next, @@iterator, and @@toStringTag properties",
        body: function () {
            var aip = Object.getPrototypeOf([].values());

            assert.isTrue(aip.hasOwnProperty('next'), "%ArrayIteratorPrototype% should have a next method");
            assert.isFalse(aip.hasOwnProperty(Symbol.iterator), "%ArrayIteratorPrototype% should not have a @@iterator method");
            assert.isTrue(aip.hasOwnProperty(Symbol.toStringTag), "%ArrayIteratorPrototype% should have a @@toStringTag property");

            assert.areEqual(0, aip.next.length, "next method takes zero arguments");

            assert.areEqual("Array Iterator", aip[Symbol.toStringTag], "@@toStringTag is the string value 'Array Iterator'");

            assert.throws(function () { aip.next.call(123); }, TypeError, "%ArrayIteratorPrototype%.next() throws if its 'this' is not an object", "Array Iterator.prototype.next: 'this' is not an Array Iterator object");
            assert.throws(function () { aip.next.call("o"); }, TypeError, "%ArrayIteratorPrototype%.next() throws if its 'this' is not an object", "Array Iterator.prototype.next: 'this' is not an Array Iterator object");
            assert.throws(function () { aip.next.call({ }); }, TypeError, "%ArrayIteratorPrototype%.next() throws if its 'this' is an object but not an Array Iterator object", "Array Iterator.prototype.next: 'this' is not an Array Iterator object");
        }
    },
    {
        name: "%MapIteratorPrototype% should have next, @@iterator, and @@toStringTag properties",
        body: function () {
            var mip = Object.getPrototypeOf((new Map()).values());

            assert.isTrue(mip.hasOwnProperty('next'), "%MapIteratorPrototype% should have a next method");
            assert.isFalse(mip.hasOwnProperty(Symbol.iterator), "%MapIteratorPrototype% should not have a @@iterator method");
            assert.isTrue(mip.hasOwnProperty(Symbol.toStringTag), "%MapIteratorPrototype% should have a @@toStringTag property");

            assert.areEqual(0, mip.next.length, "next method takes zero arguments");

            assert.areEqual("Map Iterator", mip[Symbol.toStringTag], "@@toStringTag is the string value 'Map Iterator'");

            assert.throws(function () { mip.next.call(123); }, TypeError, "%MapIteratorPrototype%.next() throws if its 'this' is not an object", "Map Iterator.prototype.next: 'this' is not an Map Iterator object");
            assert.throws(function () { mip.next.call("o"); }, TypeError, "%MapIteratorPrototype%.next() throws if its 'this' is not an object", "Map Iterator.prototype.next: 'this' is not an Map Iterator object");
            assert.throws(function () { mip.next.call({ }); }, TypeError, "%MapIteratorPrototype%.next() throws if its 'this' is an object but not an Map Iterator object", "Map Iterator.prototype.next: 'this' is not an Map Iterator object");
        }
    },
    {
        name: "%SetIteratorPrototype% should have next, @@iterator, and @@toStringTag properties",
        body: function () {
            var sip = Object.getPrototypeOf((new Set()).values());

            assert.isTrue(sip.hasOwnProperty('next'), "%SetIteratorPrototype% should have a next method");
            assert.isFalse(sip.hasOwnProperty(Symbol.iterator), "%SetIteratorPrototype% should not have a @@iterator method");
            assert.isTrue(sip.hasOwnProperty(Symbol.toStringTag), "%SetIteratorPrototype% should have a @@toStringTag property");

            assert.areEqual(0, sip.next.length, "next method takes zero arguments");

            assert.areEqual("Set Iterator", sip[Symbol.toStringTag], "@@toStringTag is the string value 'Set Iterator'");

            assert.throws(function () { sip.next.call(123); }, TypeError, "%SetIteratorPrototype%.next() throws if its 'this' is not an object", "Set Iterator.prototype.next: 'this' is not an Set Iterator object");
            assert.throws(function () { sip.next.call("o"); }, TypeError, "%SetIteratorPrototype%.next() throws if its 'this' is not an object", "Set Iterator.prototype.next: 'this' is not an Set Iterator object");
            assert.throws(function () { sip.next.call({ }); }, TypeError, "%SetIteratorPrototype%.next() throws if its 'this' is an object but not an Set Iterator object", "Set Iterator.prototype.next: 'this' is not an Set Iterator object");
        }
    },
    {
        name: "%StringIteratorPrototype% should have next, @@iterator, and @@toStringTag properties",
        body: function () {
            var sip = Object.getPrototypeOf(""[Symbol.iterator]());

            assert.isTrue(sip.hasOwnProperty('next'), "%StringIteratorPrototype% should have a next method");
            assert.isFalse(sip.hasOwnProperty(Symbol.iterator), "%StringIteratorPrototype% should not have a @@iterator method");
            assert.isTrue(sip.hasOwnProperty(Symbol.toStringTag), "%StringIteratorPrototype% should have a @@toStringTag property");

            assert.areEqual(0, sip.next.length, "next method takes zero arguments");

            assert.areEqual("String Iterator", sip[Symbol.toStringTag], "@@toStringTag is the string value 'String Iterator'");

            assert.throws(function () { sip.next.call(123); }, TypeError, "%StringIteratorPrototype%.next() throws if its 'this' is not an object", "String Iterator.prototype.next: 'this' is not an String Iterator object");
            assert.throws(function () { sip.next.call("o"); }, TypeError, "%StringIteratorPrototype%.next() throws if its 'this' is not an object", "String Iterator.prototype.next: 'this' is not an String Iterator object");
            assert.throws(function () { sip.next.call({ }); }, TypeError, "%StringIteratorPrototype%.next() throws if its 'this' is an object but not an String Iterator object", "String Iterator.prototype.next: 'this' is not an String Iterator object");
        }
    },
    {
        name: "Array iterator methods return new objects every time they are called but all have the same prototype",
        body: function () {
            var array = [ 'a', 'b', 'c', 'd', 'e' ];
            var iters = [ array.entries(), array.entries(), array.keys(), array.keys(), array.values(), array.values() ];

            for (var i = 0; i < iters.length; i++) {
                for (var j = i + 1; j < iters.length; j++) {
                    assert.isTrue(iters[i] !== iters[j], "Each iterator is its own object");
                }
            }

            for (var i = 0; i < iters.length - 1; i++) {
                assert.isTrue(Object.getPrototypeOf(iters[i]) === Object.getPrototypeOf(iters[i + 1]), "Each iterator has the same prototype object: %ArrayIteratorPrototype%");
            }
        }
    },
    {
        name: "Map iterator methods return new objects every time they are called but all have the same prototype",
        body: function () {
            var map = getNewMapWith12345();
            var iters = [ map.entries(), map.entries(), map.keys(), map.keys(), map.values(), map.values() ];

            for (var i = 0; i < iters.length; i++) {
                for (var j = i + 1; j < iters.length; j++) {
                    assert.isTrue(iters[i] !== iters[j], "Each iterator is its own object");
                }
            }

            for (var i = 0; i < iters.length - 1; i++) {
                assert.isTrue(Object.getPrototypeOf(iters[i]) === Object.getPrototypeOf(iters[i + 1]), "Each iterator has the same prototype object: %MapIteratorPrototype%");
            }
        }
    },
    {
        name: "Set iterator methods return new objects every time they are called but all have the same prototype",
        body: function () {
            var set = new Set(); set.add('a'); set.add('b'); set.add('c'); set.add('d'); set.add('e');
            var iters = [ set.entries(), set.entries(), set.values(), set.values() ];

            for (var i = 0; i < iters.length; i++) {
                for (var j = i + 1; j < iters.length; j++) {
                    assert.isTrue(iters[i] !== iters[j], "Each iterator is its own object");
                }
            }

            for (var i = 0; i < iters.length - 1; i++) {
                assert.isTrue(Object.getPrototypeOf(iters[i]) === Object.getPrototypeOf(iters[i + 1]), "Each iterator has the same prototype object: %MapIteratorPrototype%");
            }
        }
    },
    {
        name: "String iterator methods return new objects every time they are called but all have the same prototype",
        body: function () {
            var string = "abcde";
            var iters = [ string[Symbol.iterator](), string[Symbol.iterator]() ];

            for (var i = 0; i < iters.length; i++) {
                for (var j = i + 1; j < iters.length; j++) {
                    assert.isTrue(iters[i] !== iters[j], "Each iterator is its own object");
                }
            }

            for (var i = 0; i < iters.length - 1; i++) {
                assert.isTrue(Object.getPrototypeOf(iters[i]) === Object.getPrototypeOf(iters[i + 1]), "Each iterator has the same prototype object: %MapIteratorPrototype%");
            }
        }
    },
    {
        name: "Empty array or array-like objects give back iterators that are immediately complete",
        body: function () {
            var iter;
            var array = [ ];
            var arraylike = { length: 0 };

            iter = array.entries();
            assert.areEqual({ done: true, value: undefined }, iter.next(), "array entries iterator is initially complete for empty arrays");

            iter = array.keys();
            assert.areEqual({ done: true, value: undefined }, iter.next(), "array keys iterator is initially complete for empty arrays");

            iter = array.values();
            assert.areEqual({ done: true, value: undefined }, iter.next(), "array values iterator is initially complete for empty arrays");

            iter = Array.prototype.entries.call(arraylike);
            assert.areEqual({ done: true, value: undefined }, iter.next(), "array entries iterator is initially complete for empty array-like objects");

            iter = Array.prototype.keys.call(arraylike);
            assert.areEqual({ done: true, value: undefined }, iter.next(), "array keys iterator is initially complete for empty array-like objects");

            iter = Array.prototype.values.call(arraylike);
            assert.areEqual({ done: true, value: undefined }, iter.next(), "array values iterator is initially complete for empty array-like objects");
        }
    },
    {
        name: "Empty (new and cleared) Map objects give back iterators that are immediately complete",
        body: function () {
            var iter;
            var map = new Map();

            iter = map.entries();
            assert.areEqual({ done: true, value: undefined }, iter.next(), "map entries iterator is initially complete for empty maps (new)");

            iter = map.keys();
            assert.areEqual({ done: true, value: undefined }, iter.next(), "map keys iterator is initially complete for empty maps (new)");

            iter = map.values();
            assert.areEqual({ done: true, value: undefined }, iter.next(), "map values iterator is initially complete for empty maps (new)");

            map.set('z', 'a');
            map.set('y', 'b');
            map.set('x', 'c');
            map.clear();

            iter = map.entries();
            assert.areEqual({ done: true, value: undefined }, iter.next(), "map entries iterator is initially complete for empty maps (cleared)");

            iter = map.keys();
            assert.areEqual({ done: true, value: undefined }, iter.next(), "map keys iterator is initially complete for empty maps (cleared)");

            iter = map.values();
            assert.areEqual({ done: true, value: undefined }, iter.next(), "map values iterator is initially complete for empty maps (cleared)");
        }
    },
    {
        name: "Empty (new and cleared) Set objects give back iterators that are immediately complete",
        body: function () {
            var iter;
            var set = new Set();

            iter = set.entries();
            assert.areEqual({ done: true, value: undefined }, iter.next(), "set entries iterator is initially complete for empty sets (new)");

            iter = set.values();
            assert.areEqual({ done: true, value: undefined }, iter.next(), "set values iterator is initially complete for empty sets (new)");

            set.add('a');
            set.add('b');
            set.add('c');
            set.clear();

            iter = set.entries();
            assert.areEqual({ done: true, value: undefined }, iter.next(), "set entries iterator is initially complete for empty sets (cleared)");

            iter = set.values();
            assert.areEqual({ done: true, value: undefined }, iter.next(), "set values iterator is initially complete for empty sets (cleared)");
        }
    },
    {
        name: "Empty strings give back iterators that are immediately complete",
        body: function () {
            var iter = ""[Symbol.iterator]();
            assert.areEqual({ done: true, value: undefined }, iter.next(), "string iterator is initially complete for empty strings");
        }
    },
    {
        name: "Array.prototype.entries gives back iterator over the index-element (key-value) pairs of an array in index order",
        body: function () {
            var array = [ 'a', 'b', 'c', 'd', 'e' ];
            var iter = array.entries();

            assert.areEqual({ done: false, value: [ 0, 'a' ] }, iter.next(), "1st result of entries iterator is index 0 and element 'a' as a two element array pair");
            assert.areEqual({ done: false, value: [ 1, 'b' ] }, iter.next(), "2nd result of entries iterator is index 1 and element 'b' as a two element array pair");
            assert.areEqual({ done: false, value: [ 2, 'c' ] }, iter.next(), "3rd result of entries iterator is index 2 and element 'c' as a two element array pair");
            assert.areEqual({ done: false, value: [ 3, 'd' ] }, iter.next(), "4th result of entries iterator is index 3 and element 'd' as a two element array pair");
            assert.areEqual({ done: false, value: [ 4, 'e' ] }, iter.next(), "5th result of entries iterator is index 4 and element 'e' as a two element array pair");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "entries iterator completes after all 5 entries");

            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling any array iterator's next method after it has completed yields the same undefined result value");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling any array iterator's next method after it has completed yields the same undefined result value (checking twice)");
        }
    },
    {
        name: "Array.prototype.keys gives back iterator over the indices of an array in index order",
        body: function () {
            var array = [ 'a', 'b', 'c', 'd', 'e' ];
            var iter = array.keys();

            assert.areEqual({ done: false, value: 0 }, iter.next(), "1st result of keys iterator is index 0");
            assert.areEqual({ done: false, value: 1 }, iter.next(), "2nd result of keys iterator is index 1");
            assert.areEqual({ done: false, value: 2 }, iter.next(), "3rd result of keys iterator is index 2");
            assert.areEqual({ done: false, value: 3 }, iter.next(), "4th result of keys iterator is index 3");
            assert.areEqual({ done: false, value: 4 }, iter.next(), "5th result of keys iterator is index 4");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "keys iterator completes after all 5 keys");

            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling any array iterator's next method after it has completed yields the same undefined result value");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling any array iterator's next method after it has completed yields the same undefined result value (checking twice)");
        }
    },
    {
        name: "Array.prototype.values gives back iterator over the elements of an array in index order",
        body: function () {
            var array = [ 'a', 'b', 'c', 'd', 'e' ];
            var iter = array.values();

            assert.areEqual({ done: false, value: 'a' }, iter.next(), "1st result of values iterator is element 'a'");
            assert.areEqual({ done: false, value: 'b' }, iter.next(), "2nd result of values iterator is element 'b'");
            assert.areEqual({ done: false, value: 'c' }, iter.next(), "3rd result of values iterator is element 'c'");
            assert.areEqual({ done: false, value: 'd' }, iter.next(), "4th result of values iterator is element 'd'");
            assert.areEqual({ done: false, value: 'e' }, iter.next(), "5th result of values iterator is element 'e'");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "values iterator completes after all 5 values");

            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling any array iterator's next method after it has completed yields the same undefined result value");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling any array iterator's next method after it has completed yields the same undefined result value (checking twice)");
        }
    },
    {
        name: "Map.prototype.entries gives back iterator over the key-value pairs of a map in insertion order",
        body: function () {
            var map = getNewMapWith12345();
            var iter = map.entries();

            assert.areEqual({ done: false, value: [ 1, 6 ] }, iter.next(), "1st result of entries iterator is key 1 and value 6 as a two element array pair");
            assert.areEqual({ done: false, value: [ 2, 7 ] }, iter.next(), "2nd result of entries iterator is key 2 and value 7 as a two element array pair");
            assert.areEqual({ done: false, value: [ 3, 8 ] }, iter.next(), "3rd result of entries iterator is key 3 and value 8 as a two element array pair");
            assert.areEqual({ done: false, value: [ 4, 9 ] }, iter.next(), "4th result of entries iterator is key 4 and value 9 as a two element array pair");
            assert.areEqual({ done: false, value: [ 5, 10 ] }, iter.next(), "5th result of entries iterator is key 5 and value 10 as a two element array pair");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "entries iterator completes after all 5 entries");

            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling any map iterator's next method after it has completed yields the same undefined result value");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling any map iterator's next method after it has completed yields the same undefined result value (checking twice)");
        }
    },
    {
        name: "Map.prototype.keys gives back iterator over the keys of a map in insertion order",
        body: function () {
            var map = getNewMapWith12345();
            var iter = map.keys();

            assert.areEqual({ done: false, value: 1 }, iter.next(), "1st result of keys iterator is key 1");
            assert.areEqual({ done: false, value: 2 }, iter.next(), "2nd result of keys iterator is key 2");
            assert.areEqual({ done: false, value: 3 }, iter.next(), "3rd result of keys iterator is key 3");
            assert.areEqual({ done: false, value: 4 }, iter.next(), "4th result of keys iterator is key 4");
            assert.areEqual({ done: false, value: 5 }, iter.next(), "5th result of keys iterator is key 5");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "keys iterator completes after all 5 entries");

            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling any map iterator's next method after it has completed yields the same undefined result value");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling any map iterator's next method after it has completed yields the same undefined result value (checking twice)");
        }
    },
    {
        name: "Map.prototype.values gives back iterator over the values of a map in insertion order",
        body: function () {
            var map = getNewMapWith12345();
            var iter = map.values();

            assert.areEqual({ done: false, value: 6 }, iter.next(), "1st result of values iterator is value 6");
            assert.areEqual({ done: false, value: 7 }, iter.next(), "2nd result of values iterator is value 7");
            assert.areEqual({ done: false, value: 8 }, iter.next(), "3rd result of values iterator is value 8");
            assert.areEqual({ done: false, value: 9 }, iter.next(), "4th result of values iterator is value 9");
            assert.areEqual({ done: false, value: 10 }, iter.next(), "5th result of values iterator is value 10");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "values iterator completes after all 5 entries");

            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling any map iterator's next method after it has completed yields the same undefined result value");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling any map iterator's next method after it has completed yields the same undefined result value (checking twice)");
        }
    },
    {
        name: "Set.prototype.entries gives back iterator over the 'key-value' pairs (i.e. the values twice) of a set in insertion order",
        body: function () {
            var set = getNewSetWith12345();
            var iter = set.entries();

            assert.areEqual({ done: false, value: [ 1, 1 ] }, iter.next(), "1st result of entries iterator is key 1 and value 1 as a two element array pair");
            assert.areEqual({ done: false, value: [ 2, 2 ] }, iter.next(), "2nd result of entries iterator is key 2 and value 2 as a two element array pair");
            assert.areEqual({ done: false, value: [ 3, 3 ] }, iter.next(), "3rd result of entries iterator is key 3 and value 3 as a two element array pair");
            assert.areEqual({ done: false, value: [ 4, 4 ] }, iter.next(), "4th result of entries iterator is key 4 and value 4 as a two element array pair");
            assert.areEqual({ done: false, value: [ 5, 5 ] }, iter.next(), "5th result of entries iterator is key 5 and value 5 as a two element array pair");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "entries iterator completes after all 5 entries");

            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling any set iterator's next method after it has completed yields the same undefined result value");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling any set iterator's next method after it has completed yields the same undefined result value (checking twice)");
        }
    },
    {
        name: "Set.prototype.values gives back iterator over the values of a set in insertion order",
        body: function () {
            var set = getNewSetWith12345();
            var iter = set.values();

            assert.areEqual({ done: false, value: 1 }, iter.next(), "1st result of values iterator is value 1");
            assert.areEqual({ done: false, value: 2 }, iter.next(), "2nd result of values iterator is value 2");
            assert.areEqual({ done: false, value: 3 }, iter.next(), "3rd result of values iterator is value 3");
            assert.areEqual({ done: false, value: 4 }, iter.next(), "4th result of values iterator is value 4");
            assert.areEqual({ done: false, value: 5 }, iter.next(), "5th result of values iterator is value 5");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "values iterator completes after all 5 entries");

            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling any set iterator's next method after it has completed yields the same undefined result value");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling any set iterator's next method after it has completed yields the same undefined result value (checking twice)");
        }
    },
    {
        name: "String.prototype[Symbol.iterator] gives back iterator over the code points of a string in forward order",
        body: function () {
            var string = "abcde";
            var iter = string[Symbol.iterator]();

            assert.areEqual({ done: false, value: 'a' }, iter.next(), "1st result of string iterator is string 'a'");
            assert.areEqual({ done: false, value: 'b' }, iter.next(), "2nd result of string iterator is string 'b'");
            assert.areEqual({ done: false, value: 'c' }, iter.next(), "3rd result of string iterator is string 'c'");
            assert.areEqual({ done: false, value: 'd' }, iter.next(), "4th result of string iterator is string 'd'");
            assert.areEqual({ done: false, value: 'e' }, iter.next(), "5th result of string iterator is string 'e'");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "string iterator completes after all 5 code points");

            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling string iterator's next method after it has completed yields the same undefined result value");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling string iterator's next method after it has completed yields the same undefined result value (checking twice)");

            // a string with code points requiring surrogate pairs
            string = "ab\uD834\uDD1Ec\uD801\uDC27";
            var iter = string[Symbol.iterator]();

            assert.areEqual({ done: false, value: 'a' }, iter.next(), "1st result of string iterator is string 'a' (surrogate pairs)");
            assert.areEqual({ done: false, value: 'b' }, iter.next(), "2nd result of string iterator is string 'b' (surrogate pairs)");
            assert.areEqual({ done: false, value: '\u{1D11E}' }, iter.next(), "3rd result of string iterator is string '\\u{1D11E}' (surrogate pairs)");
            assert.areEqual({ done: false, value: 'c' }, iter.next(), "4th result of string iterator is string 'c' (surrogate pairs)");
            assert.areEqual({ done: false, value: '\uD801\uDC27' }, iter.next(), "5th result of string iterator is string '\\uD801\\uDC27' (surrogate pairs)");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "string iterator completes after all 5 code points (surrogate pairs)");

            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling string iterator's next method after it has completed yields the same undefined result value (surrogate pairs)");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "calling string iterator's next method after it has completed yields the same undefined result value (checking twice) (surrogate pairs)");
        }
    },
    {
        name: "Array Iterator can be used on array-like objects; those that have length properties",
        body: function () {
            var o = { length: 5, 0: 'a', 1: 'b', 2: 'c', 3: 'd', 4: 'e' };

            var iter = Array.prototype.values.call(o);

            assert.areEqual({ done: false, value: 'a' }, iter.next(), "1st result of values iterator on array-like object is element 'a'");
            assert.areEqual({ done: false, value: 'b' }, iter.next(), "2nd result of values iterator on array-like object is element 'b'");
            assert.areEqual({ done: false, value: 'c' }, iter.next(), "3rd result of values iterator on array-like object is element 'c'");
            assert.areEqual({ done: false, value: 'd' }, iter.next(), "4th result of values iterator on array-like object is element 'd'");
            assert.areEqual({ done: false, value: 'e' }, iter.next(), "5th result of values iterator on array-like object is element 'e'");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "values iterator on array-like object completes after all 5 values");

            // Setting the length lower should be reflected by the iterator
            o.length = 2;
            iter = Array.prototype.values.call(o);

            assert.areEqual({ done: false, value: 'a' }, iter.next(), "1st result of values iterator on array-like object is element 'a'");
            assert.areEqual({ done: false, value: 'b' }, iter.next(), "2nd result of values iterator on array-like object is element 'b'");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "values iterator on array-like object completes after 2 values");

            // Setting the length higher should also be reflected, giving undefined for the non-existent properties
            o.length = 7;
            var iter = Array.prototype.values.call(o);

            assert.areEqual({ done: false, value: 'a' }, iter.next(), "1st result of values iterator on array-like object is element 'a'");
            assert.areEqual({ done: false, value: 'b' }, iter.next(), "2nd result of values iterator on array-like object is element 'b'");
            assert.areEqual({ done: false, value: 'c' }, iter.next(), "3rd result of values iterator on array-like object is element 'c'");
            assert.areEqual({ done: false, value: 'd' }, iter.next(), "4th result of values iterator on array-like object is element 'd'");
            assert.areEqual({ done: false, value: 'e' }, iter.next(), "5th result of values iterator on array-like object is element 'e'");
            assert.areEqual({ done: false, value: undefined }, iter.next(), "Sixth result of values iterator on array-like object is element undefined");
            assert.areEqual({ done: false, value: undefined }, iter.next(), "Seventh result of values iterator on array-like object is element undefined");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "values iterator on array-like object completes after 7 values");
        }
    },
    {
        name: "Array Iterator created with an object whose length property is negative should be complete upon creation (and other funny values for length)",
        body: function () {
            var o = { length: -1, 0: 'a', 1: 'b' };

            var iter = Array.prototype[Symbol.iterator].call(o);

            assert.areEqual({ done: true, value: undefined }, iter.next(), "Iterator is initially complete for length = -1");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "Iterator remains complete length = -1");

            o.length = Number.NaN;
            iter = Array.prototype[Symbol.iterator].call(o);

            assert.areEqual({ done: true, value: undefined }, iter.next(), "Iterator is initially complete for length = NaN");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "Iterator remains complete length = NaN");

            o.length = -0;
            iter = Array.prototype[Symbol.iterator].call(o);

            assert.areEqual({ done: true, value: undefined }, iter.next(), "Iterator is initially complete for length = -0");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "Iterator remains complete length =  -0");

            o.length = Number.NEGATIVE_INFINITY;
            iter = Array.prototype[Symbol.iterator].call(o);

            assert.areEqual({ done: true, value: undefined }, iter.next(), "Iterator is initially complete for length = -Infinity");
            assert.areEqual({ done: true, value: undefined }, iter.next(), "Iterator remains complete length = -Infinity");

            o.length = Number.POSITIVE_INFINITY;
            iter = Array.prototype[Symbol.iterator].call(o);

            assert.areEqual({ done: false, value: 'a' }, iter.next(), "Iterator is not initially complete for length = +Infinity; length is capped at 2^53 - 1");
            assert.areEqual({ done: false, value: 'b' }, iter.next(), "Iterator would take a long time to complete for length = +Infinity (capped at 2^53 - 1)");
        }
    },
    {
        name: "Map iterator should enumerate all map items if any deletes occur on items that have already been enumerated",
        body: function () {
            var i = 0;
            var map = getNewMapWith12345();

            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                map.delete(key);
                i += 1;
                assert.isTrue(key == i, "map.entries() should enumerate keys 1, 2, 3, 4, 5 in that order");
                assert.isTrue(val == i + 5, "map.entries() should enumerate values 6, 7, 8, 9, 10 in that order");
            }

            for (var entry of map.entries()) {
                assert.fail("Shouldn't execute; map should be empty");
            }


            i = 0;
            map = getNewMapWith12345();

            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                if (key >= 3) {
                    map.delete(key - 2);
                }
                i += 1;
                assert.isTrue(key == i, "map.entries() should enumerate keys 1, 2, 3, 4, 5 in that order");
                assert.isTrue(val == i + 5, "map.entries() should enumerate values 6, 7, 8, 9, 10 in that order");
            }

            i = 3;
            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                i += 1;
                assert.isTrue(key == i, "map.entries() should enumerate keys 4, 5 in that order");
                assert.isTrue(val == i + 5, "map.entries() should enumerate values 9, 10 in that order");
            }
        }
    },
    {
        name: "Map iterator should not enumerate map items that are deleted during enumeration before being visited",
        body: function () {
            var i = 1;
            var map = getNewMapWith12345();

            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                assert.isTrue(key == i, "map.entries() should enumerate keys 1, 3, 5 in that order");
                assert.isTrue(val == i + 5, "map.entries() should enumerate values 6, 8, 10 in that order");
                map.delete(key + 1);
                i += 2;
            }

            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                assert.isTrue(key == 1, "map.entries() should enumerate key 1 only");
                assert.isTrue(val == 6, "map.entries() should enumerate value 6 only");
                map.delete(3);
                map.delete(5);
            }

            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                assert.isTrue(key == 1, "map.entries() should enumerate 1 only again");
                assert.isTrue(val == 6, "map.entries() should enumerate value 6 only again");
                map.delete(1);
            }

            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                assert.fail("Shouldn't execute, map should be empty");
            }


            map = getNewMapWith12345();

            i = 0;
            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                map.delete(6 - key);
                i += 1;
                assert.isTrue(key == i && key <= 3, "map.entries() should enumerate keys 1, 2, 3 in that order");
                assert.isTrue(val == i + 5 && val <= 8, "map.entries() should enumerate values 6, 7, 8 in that order");
            }

            i = 0;
            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                i += 1;
                assert.isTrue(key == i && key <= 2, "map.entries() should enumerate 1, 2 in that order");
                assert.isTrue(val == i + 5 && val <= 7, "map.entries() should enumerate values 6, 7 in that order");
            }
        }
    },
    {
        name: "Map iterator should continue to enumerate items as long as they are added but only if they were not already in the map, and changing an existing key's value doesn't change its position",
        body: function () {
            var i = 0;
            var map = new Map();
            map.set(1, 21);

            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                i += 1;
                assert.isTrue(key == i, "map.entries() should enumerate keys 1 through 20 in order");
                assert.isTrue(val == i + 20, "map.entries() should enumerate values 21 through 40 in order");
                if (key < 20)
                {
                    map.set(key + 1, val + 1);
                }
            }
            assert.isTrue(i == 20, "map.entries() should have enumerated up to 20");

            i = 0;
            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                i += 1;
                assert.isTrue(key == i, "map.entries() should only enumerate 1 through 20 in order once each, no duplicates");
                if (key < 20)
                {
                    map.set(key + 1, i);
                }
            }
            assert.isTrue(i == 20, "map.entries() should have enumerated up to 20 again");
        }
    },
    {
        name: "Map iterator should stop enumerating items if the map is cleared during enumeration",
        body: function () {
            var i = 0;
            var map = getNewMapWith12345();

            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                i += 1;
                assert.isTrue(key == i, "map.entries() should enumerate 1 and stop");
                if (key == 1)
                {
                    map.clear();
                }
            }
            assert.isTrue(i == 1, "map.entries() should have stopped after 1");

            i = 0;
            map = getNewMapWith12345();
            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                i += 1;
                assert.isTrue(key == i, "map.entries() should enumerate 1, 2 and stop");
                if (key == 2)
                {
                    map.clear();
                }
            }
            assert.isTrue(i == 2, "map.entries() should have stopped after 1, 2");

            i = 0;
            map = getNewMapWith12345();
            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                i += 1;
                assert.isTrue(key == i, "map.entries() should enumerate 1, 2, 3 and stop");
                if (key == 3)
                {
                    map.clear();
                }
            }
            assert.isTrue(i == 3, "map.entries() should have stopped after 1, 2, 3");

            i = 0;
            map = getNewMapWith12345();
            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                i += 1;
                assert.isTrue(key == i, "map.entries() should enumerate 1, 2, 3, 4 and stop");
                if (key == 4)
                {
                    map.clear();
                }
            }
            assert.isTrue(i == 4, "map.entries() should have stopped after 1, 2, 3, 4");

            i = 0;
            map = getNewMapWith12345();
            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                i += 1;
                assert.isTrue(key == i, "map.entries() should enumerate 1, 2, 3, 4, 5 and stop");
                if (key == 5)
                {
                    map.clear();
                }
            }
            assert.isTrue(i == 5, "map.entries() should have enumerated all 1, 2, 3, 4, 5");
            assert.isTrue(map.size == 0, "map should be empty");
        }
    },
    {
        name: "Map iterator should revisit items if they are removed after being visited but re-added before enumeration stops",
        body: function () {
            var i = 0;
            var map = getNewMapWith12345();

            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                if (key == 3) {
                    map.delete(2);
                    map.delete(1);
                    map.set(1);
                    map.set(2);
                }

                i += 1;
                assert.isTrue(key == i, "map.entries() should enumerate 1, 2, 3, 4, 5, 1, 2 in that order");
                if (key == 5) {
                    i = 0;
                }

            }

            i = 2;
            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                i += 1;
                assert.isTrue(key == i, "map.entries() should enumerate 3, 4, 5, 1, 2 in that order");
                if (key == 5) {
                    i = 0;
                }

            }
        }
    },
    {
        name: "Map iterator should continue enumeration indefinitely if items are repeatedly removed and re-added without end",
        body: function () {
            var map = new Map();
            map.set(1, 0);
            map.set(2, 1);

            var keys = [ 1, 2, 1, 2, 1, 2, 1, 2, 1, 2 ];
            var i = 0;

            for (var entry of map.entries()) {
                var key = entry[0];
                var val = entry[1];

                if (i < 9) {
                    if (key == 1) {
                        map.delete(1);
                        map.set(2, i + 1);
                    } else if (key == 2) {
                        map.delete(2);
                        map.set(1, i + 1);
                    }
                }

                assert.isTrue(key == keys[i], "map.entries() should enumerate 1, 2, 1, 2, 1, 2, 1, 2, 1, 2");
                assert.isTrue(val == i, "map.entries() should enumerate values 0, 1, 2, 3, 4, 5, 6, 7, 8, 9");

                i += 1;
            }
            assert.isTrue(i == 10, "map.entries() should have called the callback 10 times");
        }
    },
    {
        name: "Set iterator should enumerate set items in insertion order and should not call the callback for empty sets",
        body: function () {
            var i = 0;
            var set = getNewSetWith12345();

            for (var val of set.values()) {
                i += 1;
                assert.isTrue(val == i, "set.values() should enumerate 1, 2, 3, 4, 5 in that order");
            }

            // a second forEach should start at the beginning again
            i = 0;
            for (var val of set.values()) {
                i += 1;
                assert.isTrue(val == i, "Repeated set.values() should enumerate 1, 2, 3, 4, 5 in that order again");
            }

            set.clear();
            for (var val of set.values()) {
                assert.fail("Shouldn't execute; set should be empty");
            }


            set = new Set();
            for (var val of set.values()) {
                assert.fail("Shouldn't execute; set should be empty");
            }

        }
    },
    {
        name: "Set iterator should enumerate all set items if any deletes occur on items that have already been enumerated",
        body: function () {
            var i = 0;
            var set = getNewSetWith12345();

            for (var val of set.values()) {
                set.delete(val);
                i += 1;
                assert.isTrue(val == i, "set.values() should enumerate 1, 2, 3, 4, 5 in that order");
            }

            for (var val of set.values()) {
                assert.fail("Shouldn't execute; set should be empty");
            }


            i = 0;
            set = getNewSetWith12345();

            for (var val of set.values()) {
                if (val >= 3) {
                    set.delete(val - 2);
                }
                i += 1;
                assert.isTrue(val == i, "set.values() should enumerate 1, 2, 3, 4, 5 in that order");
            }

            i = 3;
            for (var val of set.values()) {
                i += 1;
                assert.isTrue(val == i, "set.values() should enumerate 4, 5 in that order");
            }
        }
    },
    {
        name: "Set iterator should not enumerate set items that are deleted during enumeration before being visited",
        body: function () {
            var i = 1;
            var set = getNewSetWith12345();

            for (var val of set.values()) {
                assert.isTrue(val == i, "set.values() should enumerate 1, 3, 5 in that order");
                set.delete(val + 1);
                i += 2;
            }

            for (var val of set.values()) {
                assert.isTrue(val == 1, "set.values() should enumerate 1 only");
                set.delete(3);
                set.delete(5);
            }

            for (var val of set.values()) {
                assert.isTrue(val == 1, "set.values() should enumerate 1 only again");
                set.delete(1);
            }

            for (var val of set.values()) {
                assert.fail("Shouldn't execute, set should be empty");
            }


            set = getNewSetWith12345();

            i = 0;
            for (var val of set.values()) {
                set.delete(6 - val);
                i += 1;
                assert.isTrue(val == i && val <= 3, "set.values() should enumerate 1, 2, 3 in that order");
            }

            i = 0;
            for (var val of set.values()) {
                i += 1;
                assert.isTrue(val == i && val <= 2, "set.values() should enumerate 1, 2 in that order");
            }
        }
    },
    {
        name: "Set iterator should continue to enumerate items as long as they are added but only if they were not already in the set",
        body: function () {
            var i = 0;
            var set = new Set();
            set.add(1);

            for (var val of set.values()) {
                i += 1;
                assert.isTrue(val == i, "set.values() should enumerate 1 through 20 in order");
                if (val < 20)
                {
                    set.add(val + 1);
                }
            }
            assert.isTrue(i == 20, "set.values() should have enumerated up to 20");

            i = 0;
            for (var val of set.values()) {
                i += 1;
                assert.isTrue(val == i, "set.values() should only enumerate 1 through 20 in order once each, no duplicates");
                if (val < 20)
                {
                    set.add(val + 1);
                }
            }
            assert.isTrue(i == 20, "set.values() should have enumerated up to 20 again");
        }
    },
    {
        name: "Set iterator should stop enumerating items if the set is cleared during enumeration",
        body: function () {
            var i = 0;
            var set = getNewSetWith12345();

            for (var val of set.values()) {
                i += 1;
                assert.isTrue(val == i, "set.values() should enumerate 1 and stop");
                if (val == 1)
                {
                    set.clear();
                }
            }
            assert.isTrue(i == 1, "set.values() should have stopped after 1");

            i = 0;
            set = getNewSetWith12345();
            for (var val of set.values()) {
                i += 1;
                assert.isTrue(val == i, "set.values() should enumerate 1, 2 and stop");
                if (val == 2)
                {
                    set.clear();
                }
            }
            assert.isTrue(i == 2, "set.values() should have stopped after 1, 2");

            i = 0;
            set = getNewSetWith12345();
            for (var val of set.values()) {
                i += 1;
                assert.isTrue(val == i, "set.values() should enumerate 1, 2, 3 and stop");
                if (val == 3)
                {
                    set.clear();
                }
            }
            assert.isTrue(i == 3, "set.values() should have stopped after 1, 2, 3");

            i = 0;
            set = getNewSetWith12345();
            for (var val of set.values()) {
                i += 1;
                assert.isTrue(val == i, "set.values() should enumerate 1, 2, 3, 4 and stop");
                if (val == 4)
                {
                    set.clear();
                }
            }
            assert.isTrue(i == 4, "set.values() should have stopped after 1, 2, 3, 4");

            i = 0;
            set = getNewSetWith12345();
            for (var val of set.values()) {
                i += 1;
                assert.isTrue(val == i, "set.values() should enumerate 1, 2, 3, 4, 5 and stop");
                if (val == 5)
                {
                    set.clear();
                }
            }
            assert.isTrue(i == 5, "set.values() should have enumerated all 1, 2, 3, 4, 5");
            assert.isTrue(set.size == 0, "set should be empty");
        }
    },
    {
        name: "Set iterator should revisit items if they are removed after being visited but re-added before enumeration stops",
        body: function () {
            var i = 0;
            var set = getNewSetWith12345();

            for (var val of set.values()) {
                if (val == 3) {
                    set.delete(2);
                    set.delete(1);
                    set.add(1);
                    set.add(2);
                }

                i += 1;
                assert.isTrue(val == i, "set.values() should enumerate 1, 2, 3, 4, 5, 1, 2 in that order");
                if (val == 5) {
                    i = 0;
                }

            }

            i = 2;
            for (var val of set.values()) {
                i += 1;
                assert.isTrue(val == i, "set.values() should enumerate 3, 4, 5, 1, 2 in that order");
                if (val == 5) {
                    i = 0;
                }

            }
        }
    },
    {
        name: "Set iterator should continue enumeration indefinitely if items are repeatedly removed and re-added without end",
        body: function () {
            var set = new Set();
            set.add(1);
            set.add(2);

            var vals = [ 1, 2, 1, 2, 1, 2, 1, 2, 1, 2 ];
            var i = 0;

            for (var val of set.values()) {
                if (i < 9) {
                    if (val == 1) {
                        set.delete(1);
                        set.add(2);
                    } else if (val == 2) {
                        set.delete(2);
                        set.add(1);
                    }
                }

                assert.isTrue(val == vals[i], "set.values() should enumerate 1, 2, 1, 2, 1, 2, 1, 2, 1, 2");

                i += 1;
            }
            assert.isTrue(i == 10, "set.values() should have called the callback 10 times");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
