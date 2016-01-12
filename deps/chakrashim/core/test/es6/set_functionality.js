//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Functional Set tests -- verifies the APIs work correctly

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function getNewSetWith12345() {
    var set = new Set();
    set.add(1);
    set.add(2);
    set.add(3);
    set.add(4);
    set.add(5);

    return set;
}

var globalObject = this;

var tests = [
    {
        name: "Set constructor called on undefined or Set.prototype returns new Set object (and throws on null)",
        body: function () {
            // Set is no longer allowed to be called as a function unless the object it is given
            // for its this argument already has the [[SetData]] property on it.
            // TODO: When we implement @@create support, update this test to reflect it.
            //
            // For IE11 we simply throw if Set() is called as a function instead of in a new expression
            assert.throws(function () { Set.call(undefined); }, TypeError, "Set.call() throws TypeError given undefined");
            assert.throws(function () { Set.call(null); }, TypeError, "Set.call() throws TypeError given null");
            assert.throws(function () { Set.call(Set.prototype); }, TypeError, "Set.call() throws TypeError given Set.prototype");
            /*
            var set1 = Set.call(undefined);
            assert.isTrue(set1 !== null && set1 !== undefined && set1 !== Set.prototype, "Set constructor creates new Set object when this is undefined");

            var set2 = Set.call(Set.prototype);
            assert.isTrue(set2 !== null && set2 !== undefined && set2 !== Set.prototype, "Set constructor creates new Set object when this is equal to Set.prototype");

            var o = { };
            Object.preventExtensions(o);

            assert.throws(function () { Set.call(null); }, TypeError, "Set constructor throws on null");
            assert.throws(function () { Set.call(o); }, TypeError, "Set constructor throws on non-extensible object");
            */
        }
    },

    {
        name: "Set constructor throws when called on already initialized Set object",
        body: function () {
            var set = new Set();
            assert.throws(function () { Set.call(set); }, TypeError);

            // Set is no longer allowed to be called as a function unless the object it is given
            // for its this argument already has the [[SetData]] property on it.
            // TODO: When we implement @@create support, update this test to reflect it.
            /*
            var obj = {};
            Set.call(obj);
            assert.throws(function () { Set.call(obj); }, TypeError);

            function MySet() {
                Set.call(this);
            }
            MySet.prototype = new Set();
            MySet.prototype.constructor = MySet;

            var myset = new MySet();
            assert.throws(function () { Set.call(myset); }, TypeError);
            assert.throws(function () { MySet.call(myset); }, TypeError);
            */
        }
    },

    {
        name: "Set constructor populates the set with values from given optional iteratable argument",
        body: function () {
            var s = new Set([ 'a', 'b', 'c' ]);

            assert.areEqual(3, s.size, "s is initialized with three entries");
            assert.isTrue(s.has('a'), "s has value 'a'");
            assert.isTrue(s.has('b'), "s has value 'b'");
            assert.isTrue(s.has('c'), "s has value 'c'");

            var customIterable = {
                [Symbol.iterator]: function () {
                    var i = 1;
                    return {
                        next: function () {
                            return {
                                done: i > 4,
                                value: i++ * 2
                            };
                        }
                    };
                }
            };

            s = new Set(customIterable);

            assert.areEqual(4, s.size, "s is initialized with four entries");
            assert.isTrue(s.has(2), "s has value 2");
            assert.isTrue(s.has(4), "s has value 4");
            assert.isTrue(s.has(6), "s has value 6");
            assert.isTrue(s.has(8), "s has value 8");
        }
    },

    {
        name: "Set constructor throws exceptions for non- and malformed iterable arguments",
        body: function () {
            var iterableNoIteratorMethod = { [Symbol.iterator]: 123 };
            var iterableBadIteratorMethod = { [Symbol.iterator]: function () { } };
            var iterableNoIteratorNextMethod = { [Symbol.iterator]: function () { return { }; } };
            var iterableBadIteratorNextMethod = { [Symbol.iterator]: function () { return { next: 123 }; } };
            var iterableNoIteratorResultObject = { [Symbol.iterator]: function () { return { next: function () { } }; } };

            assert.throws(function () { new Set(123); }, TypeError, "new Set() throws on non-object", "Function expected");
            assert.throws(function () { new Set({ }); }, TypeError, "new Set() throws on non-iterable object", "Function expected");
            assert.throws(function () { new Set(iterableNoIteratorMethod); }, TypeError, "new Set() throws on non-iterable object where @@iterator property is not a function", "Function expected");
            assert.throws(function () { new Set(iterableBadIteratorMethod); }, TypeError, "new Set() throws on non-iterable object where @@iterator function doesn't return an iterator", "Object expected");
            assert.throws(function () { new Set(iterableNoIteratorNextMethod); }, TypeError, "new Set() throws on iterable object where iterator object does not have next property", "Function expected");
            assert.throws(function () { new Set(iterableBadIteratorNextMethod); }, TypeError, "new Set() throws on iterable object where iterator object's next property is not a function", "Function expected");
            assert.throws(function () { new Set(iterableNoIteratorResultObject); }, TypeError, "new Set() throws on iterable object where iterator object's next method doesn't return an iterator result", "Object expected");
        }
    },

    {
        name: "APIs throw TypeError where specified",
        body: function () {
            function MySetImposter() { }
            MySetImposter.prototype = new Set();
            MySetImposter.prototype.constructor = MySetImposter;

            var o = new MySetImposter();

            assert.throws(function () { o.add(1); }, TypeError, "add should throw if this doesn't have SetData property");
            assert.throws(function () { o.clear(); }, TypeError, "clear should throw if this doesn't have SetData property");
            assert.throws(function () { o.delete(1); }, TypeError, "delete should throw if this doesn't have SetData property");
            assert.throws(function () { o.forEach(function (k, v, s) { }); }, TypeError, "forEach should throw if this doesn't have SetData property");
            assert.throws(function () { o.has(1); }, TypeError, "has should throw if this doesn't have SetData property");
            assert.throws(function () { WScript.Echo(o.size); }, TypeError, "size should throw if this doesn't have SetData property");

            assert.throws(function () { Set.prototype.add.call(); }, TypeError, "add should throw if called with no arguments");
            assert.throws(function () { Set.prototype.clear.call(); }, TypeError, "clear should throw if called with no arguments");
            assert.throws(function () { Set.prototype.delete.call(); }, TypeError, "delete should throw if called with no arguments");
            assert.throws(function () { Set.prototype.forEach.call(); }, TypeError, "forEach should throw if called with no arguments");
            assert.throws(function () { Set.prototype.has.call(); }, TypeError, "has should throw if called with no arguments");
            assert.throws(function () { Object.getOwnPropertyDescriptor(Set.prototype, "size").get.call(); }, TypeError, "size should throw if called with no arguments");

            assert.throws(function () { Set.prototype.add.call(null, 1); }, TypeError, "add should throw if this is null");
            assert.throws(function () { Set.prototype.clear.call(null); }, TypeError, "clear should throw if this is null");
            assert.throws(function () { Set.prototype.delete.call(null, 1); }, TypeError, "delete should throw if this is null");
            assert.throws(function () { Set.prototype.forEach.call(null, function (k, v, s) { }); }, TypeError, "forEach should throw if this is null");
            assert.throws(function () { Set.prototype.has.call(null, 1); }, TypeError, "has should throw if this is null");
            assert.throws(function () { Object.getOwnPropertyDescriptor(Set.prototype, "size").get.call(null); }, TypeError, "size should throw if this is null");

            assert.throws(function () { Set.prototype.add.call(undefined, 1); }, TypeError, "add should throw if this is undefined");
            assert.throws(function () { Set.prototype.clear.call(undefined); }, TypeError, "clear should throw if this is undefined");
            assert.throws(function () { Set.prototype.delete.call(undefined, 1); }, TypeError, "delete should throw if this is undefined");
            assert.throws(function () { Set.prototype.forEach.call(undefined, function (k, v, s) { }); }, TypeError, "forEach should throw if this is undefined");
            assert.throws(function () { Set.prototype.has.call(undefined, 1); }, TypeError, "has should throw if this is undefined");
            assert.throws(function () { Object.getOwnPropertyDescriptor(Set.prototype, "size").get.call(undefined); }, TypeError, "size should throw if this is undefined");

            var set = new Set();
            assert.throws(function () { set.forEach(null); }, TypeError, "forEach should throw if its first argument is not callable, e.g. null");
            assert.throws(function () { set.forEach(undefined); }, TypeError, "forEach should throw if its first argument is not callable, e.g. undefined");
            assert.throws(function () { set.forEach(true); }, TypeError, "forEach should throw if its first argument is not callable, e.g. a boolean");
            assert.throws(function () { set.forEach(10); }, TypeError, "forEach should throw if its first argument is not callable, e.g. a number");
            assert.throws(function () { set.forEach("hello"); }, TypeError, "forEach should throw if its first argument is not callable, e.g. a string");
        }
    },

    {
        name: "Basic usage, add, clear, delete, has, size",
        body: function () {
            var set = new Set();

            assert.isTrue(set.size === 0, "Initially empty");

            set.add(1);
            set.add(2);
            set.add("Hello");
            var o = {};
            set.add(o);

            assert.isTrue(set.has(1), "Should contain 1");
            assert.isTrue(set.has(2), "Should contain 2");
            assert.isTrue(set.has("Hello"), "Should contain \"Hello\"");
            assert.isTrue(set.has(o), "Should contain o");

            assert.isTrue(set.size === 4, "Should contain four values");

            assert.isFalse(set.has(0), "Shouldn't contain other values");
            assert.isFalse(set.has("goodbye"), "Shouldn't contain other values");
            assert.isFalse(set.has(set), "Shouldn't contain other values");

            set.clear();

            assert.isTrue(set.size === 0, "Should be empty again");
            assert.isFalse(set.has(1), "Should no longer contain 1");
            assert.isFalse(set.has(2), "Should no longer contain 2");
            assert.isFalse(set.has("Hello"), "Should no longer contain \"Hello\"");
            assert.isFalse(set.has(o), "Should no longer contain o");

            set.add(1);
            set.add(2);
            set.add("Hello");
            set.add(o);

            assert.isTrue(set.has(1), "Should contain 1 again");
            assert.isTrue(set.has(2), "Should contain 2 again");
            assert.isTrue(set.has("Hello"), "Should contain \"Hello\" again");
            assert.isTrue(set.has(o), "Should contain o again");

            assert.isTrue(set.size === 4, "Should contain four values again");

            set.delete(2);

            assert.isTrue(set.has(1), "Should still contain 1");
            assert.isFalse(set.has(2), "Should no longer contain 2");
            assert.isTrue(set.has("Hello"), "Should still contain \"Hello\"");
            assert.isTrue(set.has(o), "Should still contain o");

            assert.isTrue(set.size === 3, "Should contain three values now");

            set.delete(o);
            set.delete("Hello");

            assert.isTrue(set.has(1), "Should still contain 1");
            assert.isFalse(set.has(2), "Should no longer contain 2");
            assert.isFalse(set.has("Hello"), "Should no longer contain \"Hello\"");
            assert.isFalse(set.has(o), "Should no longer contain o");

            assert.isTrue(set.size === 1, "Should contain one value now");

            set.delete(1);

            assert.isFalse(set.has(1), "Should no longer contain 1");
            assert.isTrue(set.size === 0, "Should be empty again");
        }
    },

    {
        name: "Not specifying arguments should default them to undefined",
        body: function () {
            var set = new Set();

            assert.isFalse(set.has(), "Should not have undefined");
            assert.isFalse(set.delete(), "undefined is not in the set, delete should return false");

            set.add();
            assert.isTrue(set.has(), "Should have undefined");
            assert.isTrue(set.delete(), "undefined is in the set, delete should return true");
        }
    },

    {
        name: "Extra arguments should be ignored",
        body: function () {
            var set = new Set();

            assert.isFalse(set.has(1, 2, 3), "Looks for 1, ignores 2 and 3, set is empty so should return false");
            assert.isFalse(set.delete(1, 2, 3), "Tries to delete 1, ignores 2 and 3, set is empty so should return false");

            // 2 and 3 should be ignored and not added to the set
            set.add(1, 2, 3);

            assert.isTrue(set.has(1), "Should contain 1");
            assert.isFalse(set.has(2), "Should not contain 2");
            assert.isFalse(set.has(3), "Should not contain 3");
            assert.isTrue(set.has(1, 2, 3), "Should contain 1, has should ignore 2 and 3");
            assert.isFalse(set.has(2, 1, 3), "Should not contain 2, has should ignore 1 and 3");

            assert.isFalse(set.delete(2, 1, 3), "2 is not found so should return false, ignores 1 and 3");
            assert.isFalse(set.delete(3, 1), "3 is not found so should return false, ignores 1");
            assert.isTrue(set.delete(1, 2, 3), "1 is found and deleted, so should return true, ignores 2 and 3");
        }
    },

    {
        name: "Delete should return true if item was in set, false if not",
        body: function () {
            var set = new Set();

            set.add(1);

            assert.isFalse(set.delete(2), "2 is not in the set, delete should return false");
            assert.isTrue(set.delete(1), "1 is in the set, delete should return true");
            assert.isFalse(set.delete(1), "1 is no longer in the set, delete should now return false");
        }
    },

    {
        name: "Adding the same value twice is valid",
        body: function () {
            var set = new Set();

            set.add(1);
            set.add(1);
            set.add(2);
            set.delete(1);
            set.add(2);
            set.add(1);
            set.add(1);
        }
    },

    {
        name: "clear returns undefined, add returns the set instance itself",
        body: function () {
            var set = new Set();

            assert.areEqual(set, set.add(1), "Adding new element should return Set instance");
            assert.areEqual(set, set.add(1), "Adding existing element should return Set instance");
            assert.areEqual(undefined, set.clear(), "Clearing set should return undefined");
        }
    },

    {
        name: "Value comparison is implemented according to SameValueZero algorithm defined in spec (i.e. not by object reference identity)",
        body: function () {
            var set = new Set();

            set.add(3.14159);
            set.add("hello");
            set.add(8589934592);

            assert.isTrue(set.has(3.14159), "Set contains floating point number");
            assert.isTrue(set.has(3.0 + 0.14159), "Set contains floating point number even if calculated differently");
            assert.isTrue(set.has("hello"), "Set contains string");
            assert.isTrue(set.has("hel" + "lo"), "Set contains string even if differnt reference identity");
            assert.isTrue(set.has(8589934592), "Set contains 64 bit integer value");
            assert.isTrue(set.has(65536 + 8589869056), "Set contains 64 bit integer value even if calculated diffrently");

            set.add(-0);
            assert.isTrue(set.has(-0), "Set contains -0");
            assert.isTrue(set.has(+0), "Set contains +0");
            set.add(0);
            assert.isTrue(set.has(-0), "Set still contains -0");
            assert.isTrue(set.has(+0), "Set still contains +0");
            set.delete(-0);
            assert.isFalse(set.has(-0), "Set does not contain -0");
            assert.isFalse(set.has(+0), "Set does not contain +0");

            set.add(+0);
            assert.isTrue(set.has(-0), "Set contains -0");
            assert.isTrue(set.has(+0), "Set contains +0");
            set.add(-0);
            assert.isTrue(set.has(-0), "Set still contains -0");
            assert.isTrue(set.has(+0), "Set still contains +0");
            set.delete(0);
            assert.isFalse(set.has(-0), "Set does not contain -0");
            assert.isFalse(set.has(+0), "Set does not contain +0");


            set.add(Number.NEGATIVE_INFINITY);
            assert.isTrue(set.has(Number.NEGATIVE_INFINITY), "Set contains negative infinity");
            assert.isFalse(set.has(Number.POSITIVE_INFINITY), "Set does not contain positive infinity");
            set.add(Infinity);
            assert.isTrue(set.has(Number.NEGATIVE_INFINITY), "Set contains negative infinity");
            assert.isTrue(set.has(Number.POSITIVE_INFINITY), "Set contains positive infinity");
            set.delete(Number.NEGATIVE_INFINITY);
            assert.isFalse(set.has(Number.NEGATIVE_INFINITY), "Set does not contain negative infinity");
            assert.isTrue(set.has(Number.POSITIVE_INFINITY), "Set contains positive infinity");

            assert.isFalse(set.has(NaN), "Set does not contain NaN");
            set.add(NaN);
            assert.isTrue(set.has(NaN), "Set contains NaN");
            assert.isTrue(set.has(parseInt("blah")), "Set contains NaN resulting from parseInt(\"Blah\")");
            assert.isTrue(set.has(Math.sqrt(-1)), "Set contains NaN resulting from Math.sqrt(-1)");
            assert.isTrue(set.has(0 * Infinity), "Set contains NaN resulting from 0 * Infinity");
        }
    },

    {
        name: "forEach should set the this value of the callback correctly",
        body: function () {
            var set = new Set();
            set.add(1);

            set.forEach(function (key, val, set) {
                assert.isTrue(this === globalObject, "set.forEach should use undefined as value of this keyword if second argument is not specified which is converted to the global object");
            });

            var o = { };
            set.forEach(function (key, val, set) {
                assert.isTrue(this === o, "set.forEach should use second argument if specified as value of this keyword");
            }, o);

            set.forEach(function (key, val, set) {
                assert.isTrue(this.valueOf() === 10, "set.forEach should use second argument if specified as value of this keyword even if it is a non-object (which will be converted to an object)");
            }, 10);
        }
    },

    {
        name: "forEach should enumerate set items in insertion order and should not call the callback for empty sets",
        body: function () {
            var i = 0;
            var set = getNewSetWith12345();
            var didExecute = false;

            set.forEach(function (key, val, set) {
                i += 1;
                assert.isTrue(val == i, "set.forEach should enumerate 1, 2, 3, 4, 5 in that order");
                didExecute = true;
            });
            assert.isTrue(didExecute, "set.forEach should have enumerated items");

            // a second forEach should start at the beginning again
            i = 0;
            didExecute = false;
            set.forEach(function (key, val, set) {
                i += 1;
                assert.isTrue(val == i, "Repeated set.forEach should enumerate 1, 2, 3, 4, 5 in that order again");
                didExecute = true;
            });
            assert.isTrue(didExecute, "set.forEach should have enumerated items");

            set.clear();
            set.forEach(function (key, val, set) {
                assert.fail("Shouldn't execute; set should be empty");
            });


            set = new Set();
            set.forEach(function (key, val, set) {
                assert.fail("Shouldn't execute; set should be empty");
            });

        }
    },

    {
        name: "forEach should enumerate all set items if any deletes occur on items that have already been enumerated",
        body: function () {
            var i = 0;
            var set = getNewSetWith12345();
            var didExecute = false;

            set.forEach(function (key, val, set) {
                set.delete(val);
                i += 1;
                assert.isTrue(val == i, "set.forEach should enumerate 1, 2, 3, 4, 5 in that order");
                didExecute = true;
            });
            assert.isTrue(didExecute, "set.forEach should have enumerated items");

            set.forEach(function (key, val, set) {
                assert.fail("Shouldn't execute; set should be empty");
            });


            i = 0;
            set = getNewSetWith12345();

            didExecute = false;
            set.forEach(function (key, val, set) {
                if (val >= 3) {
                    set.delete(val - 2);
                }
                i += 1;
                assert.isTrue(val == i, "set.forEach should enumerate 1, 2, 3, 4, 5 in that order");
                didExecute = true;
            });
            assert.isTrue(didExecute, "set.forEach should have enumerated items");

            i = 3;
            didExecute = false;
            set.forEach(function (key, val, set) {
                i += 1;
                assert.isTrue(val == i, "set.forEach should enumerate 4, 5 in that order");
                didExecute = true;
            });
            assert.isTrue(didExecute, "set.forEach should have enumerated items");
        }
    },

    {
        name: "forEach should not enumerate set items that are deleted during enumeration before being visited",
        body: function () {
            var i = 1;
            var set = getNewSetWith12345();
            var didExecute = false;

            set.forEach(function (key, val, set) {
                assert.isTrue(val == i, "set.forEach should enumerate 1, 3, 5 in that order");
                set.delete(val + 1);
                i += 2;
                didExecute = true;
            });
            assert.isTrue(didExecute, "set.forEach should have enumerated items");

            didExecute = false;
            set.forEach(function (key, val, set) {
                assert.isTrue(val == 1, "set.forEach should enumerate 1 only");
                set.delete(3);
                set.delete(5);
                didExecute = true;
            });
            assert.isTrue(didExecute, "set.forEach should have enumerated items");

            didExecute = false;
            set.forEach(function (key, val, set) {
                assert.isTrue(val == 1, "set.forEach should enumerate 1 only again");
                set.delete(1);
                didExecute = true;
            });
            assert.isTrue(didExecute, "set.forEach should have enumerated items");

            set.forEach(function (key, val, set) {
                assert.fail("Shouldn't execute, set should be empty");
            });


            set = getNewSetWith12345();

            i = 0;
            didExecute = false;
            set.forEach(function (key, val, set) {
                set.delete(6 - val);
                i += 1;
                assert.isTrue(val == i && val <= 3, "set.forEach should enumerate 1, 2, 3 in that order");
                didExecute = true;
            });
            assert.isTrue(didExecute, "set.forEach should have enumerated items");

            i = 0;
            didExecute = false;
            set.forEach(function (key, val, set) {
                i += 1;
                assert.isTrue(val == i && val <= 2, "set.forEach should enumerate 1, 2 in that order");
                didExecute = true;
            });
            assert.isTrue(didExecute, "set.forEach should have enumerated items");
        }
    },

    {
        name: "forEach should continue to enumerate items as long as they are added but only if they were not already in the set",
        body: function () {
            var i = 0;
            var set = new Set();
            set.add(1);

            set.forEach(function (key, val, set) {
                i += 1;
                assert.isTrue(val == i, "set.forEach should enumerate 1 through 20 in order");
                if (val < 20)
                {
                    set.add(val + 1);
                }
            });
            assert.isTrue(i == 20, "set.forEach should have enumerated up to 20");

            i = 0;
            set.forEach(function (key, val, set) {
                i += 1;
                assert.isTrue(val == i, "set.forEach should only enumerate 1 through 20 in order once each, no duplicates");
                if (val < 20)
                {
                    set.add(val + 1);
                }
            });
            assert.isTrue(i == 20, "set.forEach should have enumerated up to 20 again");
        }
    },

    {
        name: "forEach should stop enumerating items if the set is cleared during enumeration",
        body: function () {
            var i = 0;
            var set = getNewSetWith12345();

            set.forEach(function (key, val, set) {
                i += 1;
                assert.isTrue(val == i, "set.forEach should enumerate 1 and stop");
                if (val == 1)
                {
                    set.clear();
                }
            });
            assert.isTrue(i == 1, "set.forEach should have stopped after 1");

            i = 0;
            set = getNewSetWith12345();
            set.forEach(function (key, val, set) {
                i += 1;
                assert.isTrue(val == i, "set.forEach should enumerate 1, 2 and stop");
                if (val == 2)
                {
                    set.clear();
                }
            });
            assert.isTrue(i == 2, "set.forEach should have stopped after 1, 2");

            i = 0;
            set = getNewSetWith12345();
            set.forEach(function (key, val, set) {
                i += 1;
                assert.isTrue(val == i, "set.forEach should enumerate 1, 2, 3 and stop");
                if (val == 3)
                {
                    set.clear();
                }
            });
            assert.isTrue(i == 3, "set.forEach should have stopped after 1, 2, 3");

            i = 0;
            set = getNewSetWith12345();
            set.forEach(function (key, val, set) {
                i += 1;
                assert.isTrue(val == i, "set.forEach should enumerate 1, 2, 3, 4 and stop");
                if (val == 4)
                {
                    set.clear();
                }
            });
            assert.isTrue(i == 4, "set.forEach should have stopped after 1, 2, 3, 4");

            i = 0;
            set = getNewSetWith12345();
            set.forEach(function (key, val, set) {
                i += 1;
                assert.isTrue(val == i, "set.forEach should enumerate 1, 2, 3, 4, 5 and stop");
                if (val == 5)
                {
                    set.clear();
                }
            });
            assert.isTrue(i == 5, "set.forEach should have enumerated all 1, 2, 3, 4, 5");
            assert.isTrue(set.size == 0, "set should be empty");
        }
    },

    {
        name: "forEach should revisit items if they are removed after being visited but re-added before enumeration stops",
        body: function () {
            var i = 0;
            var didExecute = false;
            var set = getNewSetWith12345();

            set.forEach(function (key, val, set) {
                if (val == 3) {
                    set.delete(2);
                    set.delete(1);
                    set.add(1);
                    set.add(2);
                }

                i += 1;
                assert.isTrue(val == i, "set.forEach should enumerate 1, 2, 3, 4, 5, 1, 2 in that order");
                if (val == 5) {
                    i = 0;
                }

                didExecute = true;
            });
            assert.isTrue(didExecute, "set.forEach should have enumerated items");

            i = 2;
            didExecute = false;
            set.forEach(function (key, val, set) {
                i += 1;
                assert.isTrue(val == i, "set.forEach should enumerate 3, 4, 5, 1, 2 in that order");
                if (val == 5) {
                    i = 0;
                }

                didExecute = true;
            });
            assert.isTrue(didExecute, "set.forEach should have enumerated items");
        }
    },

    {
        name: "forEach should continue enumeration indefinitely if items are repeatedly removed and re-added without end",
        body: function () {
            var set = new Set();
            set.add(1);
            set.add(2);

            var vals = [ 1, 2, 1, 2, 1, 2, 1, 2, 1, 2 ];
            var i = 0;

            set.forEach(function (key, val, set) {
                if (i < 9) {
                    if (val == 1) {
                        set.delete(1);
                        set.add(2);
                    } else if (val == 2) {
                        set.delete(2);
                        set.add(1);
                    }
                }

                assert.isTrue(val == vals[i], "set.forEach should enumerate 1, 2, 1, 2, 1, 2, 1, 2, 1, 2");

                i += 1;
            });
            assert.isTrue(i == 10, "set.forEach should have called the callback 10 times");
        }
    },

    {
        name: "Set.prototype.add should normalize -0 keys to +0 which is observable via Set.prototype.forEach",
        body: function() {
            var set = new Set();

            set.add(-0);

            set.forEach(function (val, key, set) {
                // do not use assert.areEqual(-0, ...) because it compares -0 and +0 as equal
                assert.isTrue(+Infinity === 1 / key && key === 0, "-0 keys are normalized to +0");
            });
        }
    },

    {
        name: "Exprgen bug 3097715: When throwing a TypeError a valid scriptContext should be used",
        body: function () {
            var func3 = function () { };
            assert.throws(function () { Array()(func3(...new Set([func3, func3]))) }, TypeError, "Should throw TypeError");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
