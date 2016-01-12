//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Functional WeakSet tests -- verifies the APIs work correctly
// Note however this does not verify the GC semantics of WeakSet

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "WeakSet constructor called on undefined or WeakSet.prototype returns new WeakSet object (and throws on null, non-extensible object)",
        body: function () {
            // WeakSet is no longer allowed to be called as a function unless the object it is given
            // for its this argument already has the [[WeakSetData]] property on it.
            // TODO: When we implement @@create support, update this test to reflect it.
            //
            assert.throws(function () { WeakSet.call(undefined); }, TypeError, "WeakSet.call() throws TypeError given undefined");
            assert.throws(function () { WeakSet.call(null); }, TypeError, "WeakSet.call() throws TypeError given null");
            assert.throws(function () { WeakSet.call(WeakSet.prototype); }, TypeError, "WeakSet.call() throws TypeError given WeakSet.prototype");
            /*
            var weakset1 = WeakSet.call(undefined);
            assert.isTrue(weakset1 !== null && weakset1 !== undefined && weakset1 !== WeakSet.prototype, "WeakSet constructor creates new WeakSet object when this is undefined");

            var weakset2 = WeakSet.call(WeakSet.prototype);
            assert.isTrue(weakset2 !== null && weakset2 !== undefined && weakset2 !== WeakSet.prototype, "WeakSet constructor creates new WeakSet object when this is equal to WeakSet.prototype");

            var o = { };
            Object.preventExtensions(o);

            assert.throws(function () { WeakSet.call(null); }, TypeError, "WeakSet constructor throws on null");
            assert.throws(function () { WeakSet.call(o); }, TypeError, "WeakSet constructor throws on non-extensible object");
            */
        }
    },

    {
        name: "WeakSet constructor throws when called on already initialized WeakSet object",
        body: function () {
            var weakset = new WeakSet();
            assert.throws(function () { WeakSet.call(weakset); }, TypeError);

            // WeakSet is no longer allowed to be called as a function unless the object it is given
            // for its this argument already has the [[WeakSetData]] property on it.
            // TODO: When we implement @@create support, update this test to reflect it.
            /*
            var obj = {};
            WeakSet.call(obj);
            assert.throws(function () { WeakSet.call(obj); }, TypeError);

            function MyWeakSet() {
                WeakSet.call(this);
            }
            MyWeakSet.prototype = new WeakSet();
            MyWeakSet.prototype.constructor = MyWeakSet;

            var myweakset = new MyWeakSet();
            assert.throws(function () { WeakSet.call(myweakset); }, TypeError);
            assert.throws(function () { MyWeakSet.call(myweakset); }, TypeError);
            */
        }
    },

    {
        name: "WeakSet constructor populates the weakset with values from given optional iteratable argument",
        body: function () {
            var keys = [ { }, { }, { }, { } ];
            var ws = new WeakSet([ keys[0], keys[1], keys[2] ]);

            assert.isTrue(ws.has(keys[0]), "ws has value keys[0]");
            assert.isTrue(ws.has(keys[1]), "ws has value keys[1]");
            assert.isTrue(ws.has(keys[2]), "ws has value keys[2]");

            var customIterable = {
                [Symbol.iterator]: function () {
                    var i = 0;
                    return {
                        next: function () {
                            return {
                                done: i > 3,
                                value: keys[i++]
                            };
                        }
                    };
                }
            };

            ws = new WeakSet(customIterable);

            assert.isTrue(ws.has(keys[0]), "ws has key keys[0]");
            assert.isTrue(ws.has(keys[1]), "ws has key keys[1]");
            assert.isTrue(ws.has(keys[2]), "ws has key keys[2]");
            assert.isTrue(ws.has(keys[3]), "ws has key keys[3]");
        }
    },

    {
        name: "WeakSet constructor throws exceptions for non- and malformed iterable arguments",
        body: function () {
            var iterableNoIteratorMethod = { [Symbol.iterator]: 123 };
            var iterableBadIteratorMethod = { [Symbol.iterator]: function () { } };
            var iterableNoIteratorNextMethod = { [Symbol.iterator]: function () { return { }; } };
            var iterableBadIteratorNextMethod = { [Symbol.iterator]: function () { return { next: 123 }; } };
            var iterableNoIteratorResultObject = { [Symbol.iterator]: function () { return { next: function () { } }; } };

            assert.throws(function () { new WeakSet(123); }, TypeError, "new WeakSet() throws on non-object", "Function expected");
            assert.throws(function () { new WeakSet({ }); }, TypeError, "new WeakSet() throws on non-iterable object", "Function expected");
            assert.throws(function () { new WeakSet(iterableNoIteratorMethod); }, TypeError, "new WeakSet() throws on non-iterable object where @@iterator property is not a function", "Function expected");
            assert.throws(function () { new WeakSet(iterableBadIteratorMethod); }, TypeError, "new WeakSet() throws on non-iterable object where @@iterator function doesn't return an iterator", "Object expected");
            assert.throws(function () { new WeakSet(iterableNoIteratorNextMethod); }, TypeError, "new WeakSet() throws on iterable object where iterator object does not have next property", "Function expected");
            assert.throws(function () { new WeakSet(iterableBadIteratorNextMethod); }, TypeError, "new WeakSet() throws on iterable object where iterator object's next property is not a function", "Function expected");
            assert.throws(function () { new WeakSet(iterableNoIteratorResultObject); }, TypeError, "new WeakSet() throws on iterable object where iterator object's next method doesn't return an iterator result", "Object expected");
        }
    },

    {
        name: "APIs throw TypeError where specified",
        body: function () {
            function MyWeakSetImposter() { }
            MyWeakSetImposter.prototype = new WeakSet();
            MyWeakSetImposter.prototype.constructor = MyWeakSetImposter;

            var o = new MyWeakSetImposter();

            assert.throws(function () { o.add(o); }, TypeError, "add should throw if this doesn't have WeakSetData property");
            assert.throws(function () { o.delete(o); }, TypeError, "delete should throw if this doesn't have WeakSetData property");
            assert.throws(function () { o.has(o); }, TypeError, "has should throw if this doesn't have WeakSetData property");

            assert.throws(function () { WeakSet.prototype.add.call(); }, TypeError, "add should throw if called with no arguments");
            assert.throws(function () { WeakSet.prototype.delete.call(); }, TypeError, "delete should throw if called with no arguments");
            assert.throws(function () { WeakSet.prototype.has.call(); }, TypeError, "has should throw if called with no arguments");

            assert.throws(function () { WeakSet.prototype.add.call(null, o); }, TypeError, "add should throw if this is null");
            assert.throws(function () { WeakSet.prototype.delete.call(null, o); }, TypeError, "delete should throw if this is null");
            assert.throws(function () { WeakSet.prototype.has.call(null, o); }, TypeError, "has should throw if this is null");

            assert.throws(function () { WeakSet.prototype.add.call(undefined, o); }, TypeError, "add should throw if this is undefined");
            assert.throws(function () { WeakSet.prototype.delete.call(undefined, o); }, TypeError, "delete should throw if this is undefined");
            assert.throws(function () { WeakSet.prototype.has.call(undefined, o); }, TypeError, "has should throw if this is undefined");

            var weakset = new WeakSet();

            assert.throws(function () { weakset.add(null); }, TypeError, "add should throw if key is not an object, e.g. null");
            assert.throws(function () { weakset.add(undefined); }, TypeError, "add should throw if key is not an object, e.g. undefined");
            assert.throws(function () { weakset.add(true); }, TypeError, "add should throw if key is not an object, e.g. a boolean");
            assert.throws(function () { weakset.add(10); }, TypeError, "add should throw if key is not an object, e.g. a number");
            assert.throws(function () { weakset.add("hello"); }, TypeError, "add should throw if key is not an object, e.g. a string");
        }
    },

    {
        name: "Non-object key argument silent fails delete and has",
        body: function () {
            var weakset = new WeakSet();

            assert.isFalse(weakset.has(null), "null is not an object and cannot be a key in a WeakSet; has returns false");
            assert.isFalse(weakset.has(undefined), "undefined is not an object and cannot be a key in a WeakSet; has returns false");
            assert.isFalse(weakset.has(true), "boolean is not an object and cannot be a key in a WeakSet; has returns false");
            assert.isFalse(weakset.has(10), "number is not an object and cannot be a key in a WeakSet; has returns false");
            assert.isFalse(weakset.has("hello"), "string is not an object and cannot be a key in a WeakSet; has returns false");

            assert.isFalse(weakset.delete(null), "null is not an object and cannot be a key in a WeakSet; delete returns false");
            assert.isFalse(weakset.delete(undefined), "undefined is not an object and cannot be a key in a WeakSet; delete returns false");
            assert.isFalse(weakset.delete(true), "boolean is not an object and cannot be a key in a WeakSet; delete returns false");
            assert.isFalse(weakset.delete(10), "number is not an object and cannot be a key in a WeakSet; delete returns false");
            assert.isFalse(weakset.delete("hello"), "string is not an object and cannot be a key in a WeakSet; delete returns false");

            var booleanObject = new Boolean(true);
            var numberObject = new Number(10);
            var stringObject = new String("hello");

            weakset.add(booleanObject);
            weakset.add(numberObject);
            weakset.add(stringObject);

            assert.isFalse(weakset.has(true), "boolean is not an object and cannot be a key in a WeakSet; has returns false");
            assert.isFalse(weakset.has(10), "number is not an object and cannot be a key in a WeakSet; has returns false");
            assert.isFalse(weakset.has("hello"), "string is not an object and cannot be a key in a WeakSet; has returns false");

            assert.isFalse(weakset.delete(true), "boolean is not an object and cannot be a key in a WeakSet; delete returns false");
            assert.isFalse(weakset.delete(10), "number is not an object and cannot be a key in a WeakSet; delete returns false");
            assert.isFalse(weakset.delete("hello"), "string is not an object and cannot be a key in a WeakSet; delete returns false");
        }
    },

    {
        name: "Basic usage, add, delete, has",
        body: function () {
            var weakset = new WeakSet();

            var o = { };
            var p = { };
            var q = { };

            weakset.add(o);
            weakset.add(p);
            weakset.add(q);

            assert.isTrue(weakset.has(o), "Should contain key o");
            assert.isTrue(weakset.has(p), "Should contain key p");
            assert.isTrue(weakset.has(q), "Should contain key q");
            assert.isFalse(weakset.has(weakset), "Should not contain other keys, 'weakset'");
            assert.isFalse(weakset.has({ }), "Should not contain other keys, '{ }'");

            assert.isTrue(weakset.delete(p), "Should return true after deleting key p");

            assert.isTrue(weakset.has(o), "Should still contain key o");
            assert.isFalse(weakset.has(p), "Should no longer contain key p");
            assert.isTrue(weakset.has(q), "Should still contain key q");

            assert.isFalse(weakset.delete(p), "Should return false, p is no longer a key");

            assert.isTrue(weakset.delete(o), "Should return true after deleting key o");
            assert.isTrue(weakset.delete(q), "Should return true after deleting key q");

            assert.isFalse(weakset.has(o), "Should no longer contain key o");
            assert.isFalse(weakset.has(p), "Should still not contain key p");
            assert.isFalse(weakset.has(q), "Should no longer contain key q");
        }
    },

    {
        name: "Not specifying arguments should default them to undefined",
        body: function () {
            var weakset = new WeakSet();

            assert.throws(function () { weakset.add(); }, TypeError, "Should throw TypeError for implicit undefined; add");

            assert.isFalse(weakset.has(), "Should return false for implicit undefined; has");
            assert.isFalse(weakset.delete(), "Should return false for implicit undefined; delete");
        }
    },

    {
        name: "Extra arguments should be ignored",
        body: function () {
            var weakset = new WeakSet();
            var o = { };
            var p = { };
            var q = { };

            assert.isFalse(weakset.has(o, p, q), "Looks for o, ignores p and q, weak weakset is empty and has should return false");
            assert.isFalse(weakset.delete(o, p, q), "Looks for o, ignores p and q, weak weakset is empty and delete should return false");

            weakset.add(o, p, q);

            assert.isTrue(weakset.has(o), "Should contain o");
            assert.isFalse(weakset.has(p), "Should not contain p");
            assert.isFalse(weakset.has(q), "Should not contain q");
            assert.isTrue(weakset.has(o, p, q), "Ignores p and q, does have o");
            assert.isTrue(weakset.has(o, q, p), "Order of extra arguments has no affect, still has o");
            assert.isFalse(weakset.has(p, o), "Ignores o, does not have p");

            assert.isFalse(weakset.delete(p, o, q), "p is not found so should return false, ignores o and q");
            assert.isFalse(weakset.delete(q, o), "q is not found so should return false, ignores o");
            assert.isTrue(weakset.delete(o, p, q), "o is found and deleted, so should return true, ignores p and q");
        }
    },

    {
        name: "Delete should return true if item was in the WeakSet, false if not",
        body: function () {
            var weakset = new WeakSet();
            var o = { };
            var p = { };

            weakset.add(o);

            assert.isFalse(weakset.delete(p), "p is not a key in the weakset, delete should return false");
            assert.isTrue(weakset.delete(o), "o is a key in the weakset, delete should remove it and return true");
            assert.isFalse(weakset.delete(o), "o is no longer a key in the weakset, delete should now return false");
        }
    },

    {
        name: "Adding the same key twice is valid, having no affect on the second add",
        body: function () {
            var weakset = new WeakSet();

            var o = { };
            var p = { };

            weakset.add(o);
            weakset.add(o);
            weakset.add(p);
            weakset.delete(o);
            weakset.add(p);
            weakset.add(o);
            weakset.add(o);

            weakset.delete(o);
            weakset.delete(p);

            weakset.add(o);
            assert.isTrue(weakset.has(o), "o is in the weakset");
            weakset.add(o);
            assert.isTrue(weakset.has(o), "o is still in the weakset");
            weakset.add(p);
            assert.isTrue(weakset.has(o), "o is still in the weakset");
            assert.isTrue(weakset.has(p), "p is now in the weakset");
            weakset.delete(o);
            assert.isFalse(weakset.has(o), "o is no longer in the weakset");
            assert.isTrue(weakset.has(p), "p is still in the weakset");
            weakset.add(p);
            assert.isTrue(weakset.has(p), "p is still in the weakset");
        }
    },

    {
        name: "add returns the weakset instance itself",
        body: function () {
            var weakset = new WeakSet();
            var o = { };

            assert.areEqual(weakset, weakset.add(o), "Adding a new key should return WeakSet instance");
            assert.areEqual(weakset, weakset.add(o), "Adding an existing key should return WeakSet instance");
        }
    },

    {
        name: "Adding and removing keys from one WeakSet shouldn't affect other WeakSets",
        body: function () {
            var ws1 = new WeakSet();
            var ws2 = new WeakSet();
            var ws3 = new WeakSet();

            var o = { };
            var p = { };
            var q = { };

            ws1.add(o);
            ws1.add(p);
            ws2.add(q);

            assert.isTrue(ws1.has(o), "ws1 has o");
            assert.isFalse(ws2.has(o), "ws2 does not have o");
            assert.isFalse(ws3.has(o), "ws3 does not have o");

            assert.isTrue(ws1.has(p), "ws1 has p");
            assert.isTrue(ws2.has(q), "ws2 has q");
            assert.isFalse(ws1.has(q), "ws1 does not have q");
            assert.isFalse(ws2.has(p), "ws2 does not have p");
            assert.isFalse(ws3.has(p), "ws3 does not have p");
            assert.isFalse(ws3.has(q), "ws3 does not have q");

            ws3.add(p);
            ws3.add(q);

            assert.isTrue(ws3.has(p), "ws3 now has p");
            assert.isTrue(ws3.has(q), "ws3 now has q");
            assert.isTrue(ws1.has(p), "ws1 still has p");
            assert.isFalse(ws2.has(p), "ws2 still does not have p");
            assert.isFalse(ws1.has(q), "ws1 still does not have q");
            assert.isTrue(ws2.has(q), "ws2 still has q");

            assert.isTrue(ws1.delete(p), "p is removed from ws1");

            assert.isFalse(ws1.has(p), "ws1 no longer has p");
            assert.isTrue(ws3.has(p), "ws3 still has p");

            ws3.delete(p);
            ws3.delete(q);

            assert.isFalse(ws3.has(p), "ws3 no longer has p");
            assert.isFalse(ws3.has(q), "ws3 no longer has q");
            assert.isTrue(ws1.has(o), "ws1 still has o");
            assert.isTrue(ws2.has(q), "ws2 still has q");
        }
    },

    {
        name: "Number, Boolean, and String and other special objects should all as keys",
        body: function () {
            var weakset = new WeakSet();

            var n = new Number(1);
            var b = new Boolean(2);
            var s = new String("Hi");

            /*
               Fast DOM and HostDispatch objects are tested in the mshtml test weakset_DOMkey.html
               WinRT objects are still an open issue; they are CustomExternalObjects so they work,
               but they are proxied and the proxies are not kept alive by the outside object, only
               by internal JS references.  Further, allowing objects to be linked to the lifetime
               of a WinJS object can cause cycles between JS GC objects and WinRT COM ref counted
               objects, which are not deducible by the GC.  Therefore using WinRT objects with
               WeakSet is prone to subtle easy to make memory leak bugs.
            var fd = new FastDOM();
            var hd = new HostDispatch();
            var wrt = new WinRT();
            */

            var ab = new ArrayBuffer(32);

            weakset.add(n);
            weakset.add(b);
            weakset.add(s);
            weakset.add(ab);

            assert.isTrue(weakset.has(n), "weakset has key n which is a Number instance");
            assert.isTrue(weakset.has(b), "weakset has key b which is a Boolean instance");
            assert.isTrue(weakset.has(s), "weakset has key s which is a String instance");
            assert.isTrue(weakset.has(ab), "weakset has key ab which is an ArrayBuffer instance");

            assert.isTrue(weakset.delete(n), "Successfully delete key n which is a Number instance from weakset");
            assert.isTrue(weakset.delete(b), "Successfully delete key b which is a Boolean instance from weakset");
            assert.isTrue(weakset.delete(s), "Successfully delete key s which is a String instance from weakset");
            assert.isTrue(weakset.delete(ab), "Successfully delete key ab which is an ArrayBuffer instance from weakset");

            assert.isFalse(weakset.has(n), "weakset no longer has key n");
            assert.isFalse(weakset.has(b), "weakset no longer has key b");
            assert.isFalse(weakset.has(s), "weakset no longer has key s");
            assert.isFalse(weakset.has(ab), "weakset no longer has key ab");
        }
    },

];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
