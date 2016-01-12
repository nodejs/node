//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Functional WeakMap tests -- verifies the APIs work correctly
// Note however this does not verify the GC semantics of WeakMap

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "WeakMap constructor called on undefined or WeakMap.prototype returns new WeakMap object (and throws on null, non-extensible object)",
        body: function () {
            // WeakMap is no longer allowed to be called as a function unless the object it is given
            // for its this argument already has the [[WeakMapData]] property on it.
            //
            // For IE11 we simply throw if WeakMap() is called as a function instead of in a new expression
            assert.throws(function () { WeakMap.call(undefined); }, TypeError, "WeakMap.call() throws TypeError given undefined");
            assert.throws(function () { WeakMap.call(null); }, TypeError, "WeakMap.call() throws TypeError given null");
            assert.throws(function () { WeakMap.call(WeakMap.prototype); }, TypeError, "WeakMap.call() throws TypeError given WeakMap.prototype");
            /*
            var weakmap1 = WeakMap.call(undefined);
            assert.isTrue(weakmap1 !== null && weakmap1 !== undefined && weakmap1 !== WeakMap.prototype, "WeakMap constructor creates new WeakMap object when this is undefined");

            var weakmap2 = WeakMap.call(WeakMap.prototype);
            assert.isTrue(weakmap2 !== null && weakmap2 !== undefined && weakmap2 !== WeakMap.prototype, "WeakMap constructor creates new WeakMap object when this is equal to WeakMap.prototype");

            var o = { };
            Object.preventExtensions(o);

            assert.throws(function () { WeakMap.call(null); }, TypeError, "WeakMap constructor throws on null");
            assert.throws(function () { WeakMap.call(o); }, TypeError, "WeakMap constructor throws on non-extensible object");
            */
        }
    },

    {
        name: "WeakMap constructor throws when called on already initialized WeakMap object",
        body: function () {
            var weakmap = new WeakMap();
            assert.throws(function () { WeakMap.call(weakmap); }, TypeError);

            // WeakMap is no longer allowed to be called as a function unless the object it is given
            // for its this argument already has the [[WeakMapData]] property on it.
            /*
            var obj = {};
            WeakMap.call(obj);
            assert.throws(function () { WeakMap.call(obj); }, TypeError);

            function MyWeakMap() {
                WeakMap.call(this);
            }
            MyWeakMap.prototype = new WeakMap();
            MyWeakMap.prototype.constructor = MyWeakMap;

            var myweakmap = new MyWeakMap();
            assert.throws(function () { WeakMap.call(myweakmap); }, TypeError);
            assert.throws(function () { MyWeakMap.call(myweakmap); }, TypeError);
            */
        }
    },

    {
        name: "WeakMap constructor populates the weakmap with key-values pairs from given optional iteratable argument",
        body: function () {
            var keys = [ { }, { }, { }, { } ];
            var wm = new WeakMap([ [keys[0], 1], [keys[1], 2], [keys[2], 3] ]);

            assert.areEqual(1, wm.get(keys[0]), "wm has key keys[0] mapping to value 1");
            assert.areEqual(2, wm.get(keys[1]), "wm has key keys[1] mapping to value 2");
            assert.areEqual(3, wm.get(keys[2]), "wm has key keys[2] mapping to value 3");

            var customIterable = {
                [Symbol.iterator]: function () {
                    var i = 0;
                    return {
                        next: function () {
                            return {
                                done: i > 3,
                                value: [ keys[i], ++i * 2 ]
                            };
                        }
                    };
                }
            };

            wm = new WeakMap(customIterable);

            assert.areEqual(2, wm.get(keys[0]), "wm has key keys[0] mapping to value 2");
            assert.areEqual(4, wm.get(keys[1]), "wm has key keys[1] mapping to value 4");
            assert.areEqual(6, wm.get(keys[2]), "wm has key keys[2] mapping to value 6");
            assert.areEqual(8, wm.get(keys[3]), "wm has key keys[3] mapping to value 8");
        }
    },

    {
        name: "WeakMap constructor throws exceptions for non- and malformed iterable arguments",
        body: function () {
            var iterableNoIteratorMethod = { [Symbol.iterator]: 123 };
            var iterableBadIteratorMethod = { [Symbol.iterator]: function () { } };
            var iterableNoIteratorNextMethod = { [Symbol.iterator]: function () { return { }; } };
            var iterableBadIteratorNextMethod = { [Symbol.iterator]: function () { return { next: 123 }; } };
            var iterableNoIteratorResultObject = { [Symbol.iterator]: function () { return { next: function () { } }; } };

            assert.throws(function () { new WeakMap(123); }, TypeError, "new WeakMap() throws on non-object", "Function expected");
            assert.throws(function () { new WeakMap({ }); }, TypeError, "new WeakMap() throws on non-iterable object", "Function expected");
            assert.throws(function () { new WeakMap(iterableNoIteratorMethod); }, TypeError, "new WeakMap() throws on non-iterable object where @@iterator property is not a function", "Function expected");
            assert.throws(function () { new WeakMap(iterableBadIteratorMethod); }, TypeError, "new WeakMap() throws on non-iterable object where @@iterator function doesn't return an iterator", "Object expected");
            assert.throws(function () { new WeakMap(iterableNoIteratorNextMethod); }, TypeError, "new WeakMap() throws on iterable object where iterator object does not have next property", "Function expected");
            assert.throws(function () { new WeakMap(iterableBadIteratorNextMethod); }, TypeError, "new WeakMap() throws on iterable object where iterator object's next property is not a function", "Function expected");
            assert.throws(function () { new WeakMap(iterableNoIteratorResultObject); }, TypeError, "new WeakMap() throws on iterable object where iterator object's next method doesn't return an iterator result", "Object expected");
        }
    },

    {
        name: "APIs throw TypeError where specified",
        body: function () {
            function MyWeakMapImposter() { }
            MyWeakMapImposter.prototype = new WeakMap();
            MyWeakMapImposter.prototype.constructor = MyWeakMapImposter;

            var o = new MyWeakMapImposter();

            assert.throws(function () { o.delete(o); }, TypeError, "delete should throw if this doesn't have WeakMapData property");
            assert.throws(function () { o.get(o); }, TypeError, "get should throw if this doesn't have WeakMapData property");
            assert.throws(function () { o.has(o); }, TypeError, "has should throw if this doesn't have WeakMapData property");
            assert.throws(function () { o.set(o, o); }, TypeError, "set should throw if this doesn't have WeakMapData property");

            assert.throws(function () { WeakMap.prototype.delete.call(); }, TypeError, "delete should throw if called with no arguments");
            assert.throws(function () { WeakMap.prototype.get.call(); }, TypeError, "get should throw if called with no arguments");
            assert.throws(function () { WeakMap.prototype.has.call(); }, TypeError, "has should throw if called with no arguments");
            assert.throws(function () { WeakMap.prototype.set.call(); }, TypeError, "set should throw if called with no arguments");

            assert.throws(function () { WeakMap.prototype.delete.call(null, o); }, TypeError, "delete should throw if this is null");
            assert.throws(function () { WeakMap.prototype.get.call(null, o); }, TypeError, "get should throw if this is null");
            assert.throws(function () { WeakMap.prototype.has.call(null, o); }, TypeError, "has should throw if this is null");
            assert.throws(function () { WeakMap.prototype.set.call(null, o, o); }, TypeError, "set should throw if this is null");

            assert.throws(function () { WeakMap.prototype.delete.call(undefined, o); }, TypeError, "delete should throw if this is undefined");
            assert.throws(function () { WeakMap.prototype.get.call(undefined, o); }, TypeError, "get should throw if this is undefined");
            assert.throws(function () { WeakMap.prototype.has.call(undefined, o); }, TypeError, "has should throw if this is undefined");
            assert.throws(function () { WeakMap.prototype.set.call(undefined, o, o); }, TypeError, "set should throw if this is undefined");

            var weakmap = new WeakMap();

            assert.throws(function () { weakmap.set(null, o); }, TypeError, "set should throw if key is not an object, e.g. null");
            assert.throws(function () { weakmap.set(undefined, o); }, TypeError, "set should throw if key is not an object, e.g. undefined");
            assert.throws(function () { weakmap.set(true, o); }, TypeError, "set should throw if key is not an object, e.g. a boolean");
            assert.throws(function () { weakmap.set(10, o); }, TypeError, "set should throw if key is not an object, e.g. a number");
            assert.throws(function () { weakmap.set("hello", o); }, TypeError, "set should throw if key is not an object, e.g. a string");
        }
    },

    {
        name: "Non-object key argument silent fails delete, get, and has",
        body: function () {
            var weakmap = new WeakMap();

            assert.isTrue(weakmap.get(null) === undefined, "null is not an object and cannot be a key in a WeakMap; get returns undefined");
            assert.isTrue(weakmap.get(undefined) === undefined, "undefined is not an object and cannot be a key in a WeakMap; get returns undefined");
            assert.isTrue(weakmap.get(true) === undefined, "boolean is not an object and cannot be a key in a WeakMap; get returns undefined");
            assert.isTrue(weakmap.get(10) === undefined, "number is not an object and cannot be a key in a WeakMap; get returns undefined");
            assert.isTrue(weakmap.get("hello") === undefined, "string is not an object and cannot be a key in a WeakMap; get returns undefined");

            assert.isFalse(weakmap.has(null), "null is not an object and cannot be a key in a WeakMap; has returns false");
            assert.isFalse(weakmap.has(undefined), "undefined is not an object and cannot be a key in a WeakMap; has returns false");
            assert.isFalse(weakmap.has(true), "boolean is not an object and cannot be a key in a WeakMap; has returns false");
            assert.isFalse(weakmap.has(10), "number is not an object and cannot be a key in a WeakMap; has returns false");
            assert.isFalse(weakmap.has("hello"), "string is not an object and cannot be a key in a WeakMap; has returns false");

            assert.isFalse(weakmap.delete(null), "null is not an object and cannot be a key in a WeakMap; delete returns false");
            assert.isFalse(weakmap.delete(undefined), "undefined is not an object and cannot be a key in a WeakMap; delete returns false");
            assert.isFalse(weakmap.delete(true), "boolean is not an object and cannot be a key in a WeakMap; delete returns false");
            assert.isFalse(weakmap.delete(10), "number is not an object and cannot be a key in a WeakMap; delete returns false");
            assert.isFalse(weakmap.delete("hello"), "string is not an object and cannot be a key in a WeakMap; delete returns false");

            var booleanObject = new Boolean(true);
            var numberObject = new Number(10);
            var stringObject = new String("hello");

            weakmap.set(booleanObject, null);
            weakmap.set(numberObject, null);
            weakmap.set(stringObject, null);

            assert.isTrue(weakmap.get(true) === undefined, "boolean is not an object and cannot be a key in a WeakMap; get returns undefined");
            assert.isTrue(weakmap.get(10) === undefined, "number is not an object and cannot be a key in a WeakMap; get returns undefined");
            assert.isTrue(weakmap.get("hello") === undefined, "string is not an object and cannot be a key in a WeakMap; get returns undefined");

            assert.isFalse(weakmap.has(true), "boolean is not an object and cannot be a key in a WeakMap; has returns false");
            assert.isFalse(weakmap.has(10), "number is not an object and cannot be a key in a WeakMap; has returns false");
            assert.isFalse(weakmap.has("hello"), "string is not an object and cannot be a key in a WeakMap; has returns false");

            assert.isFalse(weakmap.delete(true), "boolean is not an object and cannot be a key in a WeakMap; delete returns false");
            assert.isFalse(weakmap.delete(10), "number is not an object and cannot be a key in a WeakMap; delete returns false");
            assert.isFalse(weakmap.delete("hello"), "string is not an object and cannot be a key in a WeakMap; delete returns false");
        }
    },

    {
        name: "Basic usage, delete, get, has, set",
        body: function () {
            var weakmap = new WeakMap();

            var o = { };
            var p = { };
            var q = { };

            weakmap.set(o, 10);
            weakmap.set(p, o);
            weakmap.set(q, q);

            assert.isTrue(weakmap.has(o), "Should contain key o");
            assert.isTrue(weakmap.has(p), "Should contain key p");
            assert.isTrue(weakmap.has(q), "Should contain key q");
            assert.isFalse(weakmap.has(weakmap), "Should not contain other keys, 'weakmap'");
            assert.isFalse(weakmap.has({ }), "Should not contain other keys, '{ }'");

            assert.isTrue(weakmap.get(o) === 10, "Should weakmap o to 10");
            assert.isTrue(weakmap.get(p) === o, "Should weakmap p to o");
            assert.isTrue(weakmap.get(q) === q, "Should weakmap q to q");
            assert.isTrue(weakmap.get(weakmap) === undefined, "Should return undefined for non-existant key, 'weakmap'");
            assert.isTrue(weakmap.get({ }) === undefined, "Should return undefined for non-existant key, '{ }'");

            assert.isTrue(weakmap.delete(p), "Should return true after deleting key p");

            assert.isTrue(weakmap.has(o), "Should still contain key o");
            assert.isFalse(weakmap.has(p), "Should no longer contain key p");
            assert.isTrue(weakmap.has(q), "Should still contain key q");

            assert.isFalse(weakmap.delete(p), "Should return false, p is no longer a key");

            assert.isTrue(weakmap.delete(o), "Should return true after deleting key o");
            assert.isTrue(weakmap.delete(q), "Should return true after deleting key q");

            assert.isFalse(weakmap.has(o), "Should no longer contain key o");
            assert.isFalse(weakmap.has(p), "Should still not contain key p");
            assert.isFalse(weakmap.has(q), "Should no longer contain key q");
        }
    },

    {
        name: "Not specifying arguments should default them to undefined",
        body: function () {
            var weakmap = new WeakMap();

            assert.isFalse(weakmap.has(), "Should return false for implicit undefined; has");
            assert.isTrue(weakmap.get() === undefined, "Should return undefined for implicit undefined; get");
            assert.isFalse(weakmap.delete(), "Should return false for implicit undefined; delete");

            assert.throws(function () { weakmap.set(); }, TypeError, "Should throw TypeError for implicit undefined; set");
        }
    },

    {
        name: "Extra arguments should be ignored",
        body: function () {
            var weakmap = new WeakMap();
            var o = { };
            var p = { };
            var q = { };

            assert.isFalse(weakmap.has(o, p, q), "Looks for o, ignores p and q, weak weakmap is empty and has should return false");
            assert.isTrue(weakmap.get(o, p, q) === undefined, "Looks for o, ignores p and q, weak weakmap is empty and get should return false");
            assert.isFalse(weakmap.delete(o, p, q), "Looks for o, ignores p and q, weak weakmap is empty and delete should return false");

            weakmap.set(o, p, q);

            assert.isTrue(weakmap.has(o), "Should contain o");
            assert.isFalse(weakmap.has(p), "Should not contain p");
            assert.isFalse(weakmap.has(q), "Should not contain q");
            assert.isTrue(weakmap.has(o, p, q), "Ignores p and q, does have o");
            assert.isTrue(weakmap.has(o, q, p), "Order of extra arguments has no affect, still has o");
            assert.isFalse(weakmap.has(p, o), "Ignores o, does not have p");

            assert.isTrue(weakmap.get(o) === p, "Should contain o and return p");
            assert.isTrue(weakmap.get(p) === undefined, "Should not contain p and return undefined");
            assert.isTrue(weakmap.get(q) === undefined, "Should not contain q and return undefined");
            assert.isTrue(weakmap.get(o, p, q) === p, "Ignores p and q, does have o, returns p");
            assert.isTrue(weakmap.get(o, q, p) === p, "Order of extra arguments has no affect, still has o, still returns p");
            assert.isTrue(weakmap.get(p, o) === undefined, "Ignores o, does not have p and returns undefined");

            assert.isFalse(weakmap.delete(p, o, q), "p is not found so should return false, ignores o and q");
            assert.isFalse(weakmap.delete(q, o), "q is not found so should return false, ignores o");
            assert.isTrue(weakmap.delete(o, p, q), "o is found and deleted, so should return true, ignores p and q");
        }
    },

    {
        name: "Delete should return true if item was in the WeakMap, false if not",
        body: function () {
            var weakmap = new WeakMap();
            var o = { };
            var p = { };

            weakmap.set(o, p);

            assert.isFalse(weakmap.delete(p), "p is not a key in the weakmap, delete should return false");
            assert.isTrue(weakmap.delete(o), "o is a key in the weakmap, delete should remove it and return true");
            assert.isFalse(weakmap.delete(o), "o is no longer a key in the weakmap, delete should now return false");
        }
    },

    {
        name: "Setting the same key twice is valid, and should modify the value",
        body: function () {
            var weakmap = new WeakMap();

            var o = { };
            var p = { };

            weakmap.set(o);
            weakmap.set(o);
            weakmap.set(p);
            weakmap.delete(o);
            weakmap.set(p);
            weakmap.set(o);
            weakmap.set(o);

            weakmap.delete(o);
            weakmap.delete(p);

            weakmap.set(o, 3);
            assert.isTrue(weakmap.get(o) === 3, "o maps to 3");
            weakmap.set(o, 4);
            assert.isTrue(weakmap.get(o) === 4, "o maps to 4");
            weakmap.set(p, 5);
            assert.isTrue(weakmap.get(o) === 4, "o still maps to 4");
            assert.isTrue(weakmap.get(p) === 5, "p maps to 5");
            weakmap.delete(o);
            assert.isTrue(weakmap.get(o) === undefined, "o is no longer in the weakmap");
            assert.isTrue(weakmap.get(p) === 5, "p still maps to 5");
            weakmap.set(p, 6);
            assert.isTrue(weakmap.get(p) === 6, "p maps to 6");
        }
    },

    {
        name: "set returns the weakmap instance itself",
        body: function () {
            var weakmap = new WeakMap();
            var o = { };

            assert.areEqual(weakmap, weakmap.set(o, o), "Setting new key should return WeakMap instance");
            assert.areEqual(weakmap, weakmap.set(o, o), "Setting existing key should return WeakMap instance");
        }
    },

    {
        name: "Adding and removing keys from one WeakMap shouldn't affect other WeakMaps",
        body: function () {
            var wm1 = new WeakMap();
            var wm2 = new WeakMap();
            var wm3 = new WeakMap();

            var o = { };
            var p = { };
            var q = { };

            wm1.set(o, o);
            wm1.set(p, q);
            wm2.set(q, o);

            assert.isTrue(wm1.has(o), "wm1 has o");
            assert.isFalse(wm2.has(o), "wm2 does not have o");
            assert.isFalse(wm3.has(o), "wm3 does not have o");

            assert.isTrue(wm1.get(o) === o, "wm1 has o map to o");
            assert.isTrue(wm2.get(o) === undefined, "wm2 does not have o and get returns undefined");
            assert.isTrue(wm3.get(o) === undefined, "wm3 does not have o and get returns undefined");

            assert.isTrue(wm1.has(p), "wm1 has p");
            assert.isTrue(wm2.has(q), "wm2 has q");
            assert.isFalse(wm1.has(q), "wm1 does not have q");
            assert.isFalse(wm2.has(p), "wm2 does not have p");
            assert.isFalse(wm3.has(p), "wm3 does not have p");
            assert.isFalse(wm3.has(q), "wm3 does not have q");

            assert.isTrue(wm1.get(p) === q, "wm1 has p map to q");
            assert.isTrue(wm2.get(q) === o, "wm2 has q map to o");
            assert.isTrue(wm1.get(q) === undefined, "wm1 does not have q and get returns undefined");
            assert.isTrue(wm2.get(p) === undefined, "wm2 does not have p and get returns undefined");
            assert.isTrue(wm3.get(p) === undefined, "wm3 does not have p and get returns undefined");
            assert.isTrue(wm3.get(q) === undefined, "wm3 does not have q and get returns undefined");

            wm3.set(p, o);
            wm3.set(q, p);

            assert.isTrue(wm3.has(p), "wm3 now has p");
            assert.isTrue(wm3.has(q), "wm3 now has q");
            assert.isTrue(wm1.has(p), "wm1 still has p");
            assert.isFalse(wm2.has(p), "wm2 still does not have p");
            assert.isFalse(wm1.has(q), "wm1 still does not have q");
            assert.isTrue(wm2.has(q), "wm2 still has q");

            assert.isTrue(wm1.delete(p), "p is removed from wm1");

            assert.isFalse(wm1.has(p), "wm1 no longer has p");
            assert.isTrue(wm3.has(p), "wm3 still has p");

            wm3.delete(p);
            wm3.delete(q);

            assert.isFalse(wm3.has(p), "wm3 no longer has p");
            assert.isFalse(wm3.has(q), "wm3 no longer has q");
            assert.isTrue(wm1.has(o), "wm1 still has o");
            assert.isTrue(wm2.has(q), "wm2 still has q");
        }
    },

    {
        name: "Number, Boolean, and String and other special objects should all as keys",
        body: function () {
            var weakmap = new WeakMap();

            var n = new Number(1);
            var b = new Boolean(2);
            var s = new String("Hi");

            /*
               Fast DOM and HostDispatch objects are tested in the mshtml test weakmap_DOMkey.html
               WinRT objects are still an open issue; they are CustomExternalObjects so they work,
               but they are proxied and the proxies are not kept alive by the outside object, only
               by internal JS references.  Further, allowing objects to be linked to the lifetime
               of a WinJS object can cause cycles between JS GC objects and WinRT COM ref counted
               objects, which are not deducible by the GC.  Therefore using WinRT objects with
               WeakMap is prone to subtle easy to make memory leak bugs.
            var fd = new FastDOM();
            var hd = new HostDispatch();
            var wrt = new WinRT();
            */

            var ab = new ArrayBuffer(32);

            weakmap.set(n, 1);
            weakmap.set(b, 2);
            weakmap.set(s, 3);
            weakmap.set(ab, 4);

            assert.isTrue(weakmap.has(n), "weakmap has key n which is a Number instance");
            assert.isTrue(weakmap.has(b), "weakmap has key b which is a Boolean instance");
            assert.isTrue(weakmap.has(s), "weakmap has key s which is a String instance");
            assert.isTrue(weakmap.has(ab), "weakmap has key ab which is an ArrayBuffer instance");

            assert.isTrue(weakmap.get(n) === 1, "weakmap has key n which is a Number instance and maps it to 1");
            assert.isTrue(weakmap.get(b) === 2, "weakmap has key b which is a Boolean instance and maps it to 2");
            assert.isTrue(weakmap.get(s) === 3, "weakmap has key s which is a String instance and maps it to 3");
            assert.isTrue(weakmap.get(ab) === 4, "weakmap has key ab which is an ArrayBuffer instance and maps it to 4");

            assert.isTrue(weakmap.delete(n), "Successfully delete key n which is a Number instance from weakmap");
            assert.isTrue(weakmap.delete(b), "Successfully delete key b which is a Boolean instance from weakmap");
            assert.isTrue(weakmap.delete(s), "Successfully delete key s which is a String instance from weakmap");
            assert.isTrue(weakmap.delete(ab), "Successfully delete key ab which is an ArrayBuffer instance from weakmap");

            assert.isFalse(weakmap.has(n), "weakmap no longer has key n");
            assert.isFalse(weakmap.has(b), "weakmap no longer has key b");
            assert.isFalse(weakmap.has(s), "weakmap no longer has key s");
            assert.isFalse(weakmap.has(ab), "weakmap no longer has key ab");
        }
    },

    {
        name: "WeakMap can add keys that are sealed and frozen (testworthy because WeakMap implementation sets internal property on key objects)",
        body: function () {
            var wm = new WeakMap();

            var sealedObj = Object.seal({ });
            var frozenObj = Object.freeze({ });

            wm.set(sealedObj, 1248);
            wm.set(frozenObj, 3927);

            assert.isTrue(wm.has(sealedObj), "WeakMap has sealed object as key");
            assert.isTrue(wm.has(frozenObj), "WeakMap has frozen object as key");

            assert.areEqual(1248, wm.get(sealedObj), "WeakMap maps sealed object key to corresponding mapped value");
            assert.areEqual(3927, wm.get(frozenObj), "WeakMap maps frozen object key to corresponding mapped value");

            var wm2 = new WeakMap();

            assert.isFalse(wm2.has(sealedObj), "Second WeakMap does not have sealed object key");
            assert.isFalse(wm2.has(frozenObj), "Second WeakMap does not have frozen object key");

            wm2.set(sealedObj, 42);
            wm2.set(frozenObj, 68);

            assert.isTrue(wm2.has(sealedObj), "Second WeakMap now has sealed object key");
            assert.isTrue(wm2.has(frozenObj), "Second WeakMap now has frozen object key");

            assert.isTrue(wm.has(sealedObj), "First WeakMap still has sealed object as key");
            assert.isTrue(wm.has(frozenObj), "First WeakMap still has frozen object as key");

            wm.delete(sealedObj);
            wm2.delete(frozenObj);

            assert.isTrue(wm2.has(sealedObj), "Second WeakMap still has sealed object key");
            assert.isFalse(wm2.has(frozenObj), "Second WeakMap no longer has frozen object key");

            assert.isFalse(wm.has(sealedObj), "First WeakMap no longer has sealed object as key");
            assert.isTrue(wm.has(frozenObj), "First WeakMap still has frozen object as key");
        }
    },

];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
