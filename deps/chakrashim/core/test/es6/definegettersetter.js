//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Basic __defineGetter__, __defineSetter__, __lookupGetter__, and __lookupSetter tests -- verifies the API properties and functionality

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var globalObject = this;

var tests = {
    test01: {
        name: "__defineGetter__ defines an accessor property with getter as specified and enumerable and configurable set to true",
        body: function () {
            var o = { };
            var result = o.__defineGetter__("a", function () { return 1234; });

            assert.isTrue(result === undefined, "__defineGetter__ should return undefined");
            assert.isTrue(o.a === 1234, "Getter should call the given function and return its value");

            var d = Object.getOwnPropertyDescriptor(o, "a");

            assert.isTrue(d.enumerable, "Getter accessor property should be enumerable");
            assert.isTrue(d.configurable, "Getter accessor property should be configurable");
        }
    },
    test02: {
        name: "__defineSetter__ defines an accessor property with getter as specified and enumerable and configurable set to true",
        body: function () {
            var o = { v: 0 };
            var result = o.__defineSetter__("a", function (v) { throw new Error(); });

            assert.isTrue(result === undefined, "__defineSetter__ should return undefined");
            assert.throws(function () { o.a = 1234; }, Error, "Setter should call the given function");

            var d = Object.getOwnPropertyDescriptor(o, "a");

            assert.isTrue(d.enumerable, "Setter accessor property should be enumerable");
            assert.isTrue(d.configurable, "Setter accessor property should be configurable");
        }
    },
    test03: {
        name: "__defineGetter__ should not assign a setter and __defineSetter__ should not define a getter",
        body: function () {
            var o = { };

            o.__defineGetter__("a", function () { return 1234; });
            o.__defineSetter__("b", function (v) { });

            var da = Object.getOwnPropertyDescriptor(o, "a");
            var db = Object.getOwnPropertyDescriptor(o, "b");

            assert.isTrue(da.set === undefined, "__defineGetter__ does not add a setter");
            assert.isTrue(db.get === undefined, "__defineSetter__ does not add a getter");

            o.a = 10;
            assert.isTrue(o.a === 1234, "Getter only property should be unaffected by uses in setter context");

            assert.isTrue(o.b === undefined, "Setter only property should return undefined if used in getter context");
        }
    },
    test04: {
        name: "get and set functions should have access to the object's properties via this",
        body: function () {
            var o = { x: 1, y: 2, z: 3 };

            o.__defineGetter__("a", function () { return this.x + this.y + this.z; });
            o.__defineSetter__("b", function (v) { this.x = v; this.y = v * 2; this.z = v * 3; });

            assert.isTrue(o.a === 6, "Getter should return 1 + 2 + 3");
            o.b = 2;
            assert.isTrue(o.a === 12, "Getter should now return 2 + 4 + 6");
        }
    },
    test05: {
        name: "__defineGetter__ and __defineSetter__ called on the same property are additive; they do not clobber previous accessor",
        body: function () {
            var o = { };

            o.__defineGetter__("a", function () { return 1; });
            o.__defineSetter__("a", function (v) { throw new Error(2); });

            o.__defineSetter__("b", function (v) { throw new Error(3); });
            o.__defineGetter__("b", function () { return 4; });

            assert.isTrue(o.a === 1, "getter in 'a' should return 1");
            assert.isTrue((function () { try { o.a = 0; } catch (e) { return e.description; } return null; })() === "2", "setter in 'a' should throw a new Error with number equal to 2");
            assert.isTrue((function () { try { o.b = 0; } catch (e) { return e.description; } return null; })() === "3", "setter in 'b' should throw a new Error with number equal to 3");
            assert.isTrue(o.b === 4, "getter in 'b' should return 4");
        }
    },
    test06: {
        name: "__defineGetter__ and __defineSetter__ only allow functions as the accessor argument",
        body: function () {
            function testBadArg(arg) {
                var o = { };

                assert.throws(function () { o.__defineGetter__("a", arg); }, TypeError, "__defineGetter__ should throw with getter function arg: " + arg);
                assert.throws(function () { o.__defineSetter__("a", arg); }, TypeError, "__defineSetter__ should throw with setter function arg: " + arg);
            }

            testBadArg(undefined);
            testBadArg(null);
            testBadArg(0);
            testBadArg(1234);
            testBadArg("hello");
            testBadArg({ a: 1, b: 2 });
            testBadArg([ 1, 2 ]);
        }
    },
    test07: {
        name: "__defineGetter__ and __defineSetter__ overwrite existing property descriptors when configurable, otherwise throws",
        body: function () {
            function testWithExistingDescriptor(descriptor) {
                var shouldThrow = descriptor.configurable ? false : true;

                var o = { };
                Object.defineProperty(o, "a", descriptor);

                var fnDefGet = function () { o.__defineGetter__("a", function () { return undefined; }); };
                var fnDefSet = function () { o.__defineSetter__("a", function (v) { }); };

                if (shouldThrow) {
                    assert.throws(fnDefGet, TypeError, "__defineGetter__ should throw when called on existing non-configurable property");
                    assert.throws(fnDefSet, TypeError, "__defineSetter__ should throw when called on existing non-configurable property");
                } else {
                    fnDefGet();
                    fnDefSet();

                    var owndesc = Object.getOwnPropertyDescriptor(o, "a");
                    assert.isFalse(owndesc.hasOwnProperty("writable"), "property should no longer be a data accessor if it happened to be");
                    assert.isFalse(owndesc.hasOwnProperty("value"), "property should no longer be a data accessor if it happened to be");
                    assert.isTrue(owndesc.get !== undefined, "property should now have a getter");
                    assert.isTrue(owndesc.set !== undefined, "property should now have a setter");
                    assert.isTrue(owndesc.configurable, "property should still be configurable");
                    assert.isTrue(owndesc.enumerable, "property should now be enumerable if it wasn't already");
                }
            }

            // generic descriptor

            testWithExistingDescriptor({ configurable: true });
            testWithExistingDescriptor({ enumerable: true });
            testWithExistingDescriptor({ configurable: true, enumerable: true });
            testWithExistingDescriptor({ configurable: false });
            testWithExistingDescriptor({ enumerable: false });
            testWithExistingDescriptor({ configurable: false, enumerable: false });

            // data descriptor

            testWithExistingDescriptor({ value: 10 });
            testWithExistingDescriptor({ writable: true });
            testWithExistingDescriptor({ value: 10, writable: true });
            testWithExistingDescriptor({ value: 10, enumerable: true });
            testWithExistingDescriptor({ writable: true, enumerable: true });
            testWithExistingDescriptor({ value: 10, writable: true, enumerable: true });
            testWithExistingDescriptor({ value: 10, configurable: true });
            testWithExistingDescriptor({ writable: true, configurable: true });
            testWithExistingDescriptor({ value: 10, writable: true, configurable: true });
            testWithExistingDescriptor({ value: 10, configurable: true, enumerable: true });
            testWithExistingDescriptor({ writable: true, configurable: true, enumerable: true });
            testWithExistingDescriptor({ value: 10, writable: true, configurable: true, enumerable: true });

            // accessor descriptor
            //
            // already handled accessor descriptors implicitly via successive calls to
            // __defineGetter__ and __defineSetter__ with the same property name
            // Just make sure non-configurable accessor descriptor cannot be changed:

            testWithExistingDescriptor({ get: function () { }, configurable: false });
            testWithExistingDescriptor({ set: function (v) { }, configurable: false });
        }
    },
    test08: {
        name: "__defineGetter__ and __defineSetter__ should work regardless whether Object.defineProperty is changed by the user or not",
        body: function () {
            var builtinDefineProperty = Object.defineProperty;
            Object.defineProperty = function (o, p, d) { throw new Error("Should not execute this"); };

            var o = { };

            o.__defineGetter__("a", function () { return 1234; });
            o.__defineSetter__("a", function (v) { throw new Error(); });

            assert.isTrue(o.a === 1234, "Getter should be assigned and execute like normal");
            assert.throws(function () { o.a = 0; }, Error, "Setter should be assigned and execute like normal");

            var d = Object.getOwnPropertyDescriptor(o, "a");

            assert.isTrue(d.get !== undefined, "Accessor descriptor has get value");
            assert.isTrue(d.set !== undefined, "Accessor descriptor has set value");
            assert.isTrue(d.configurable, "Property is configurable");
            assert.isTrue(d.enumerable, "Property is enumerable");

            Object.defineProperty = builtinDefineProperty;
        }
    },
    test09: {
        name: "__defineGetter__ and __defineSetter__ both have length 2 and __lookupGetter__ and __lookupSetter__ both have length 1",
        body: function () {
            assert.isTrue(Object.prototype.__defineGetter__.length === 2, "__defineGetter__.length should be 2");
            assert.isTrue(Object.prototype.__defineSetter__.length === 2, "__defineSetter__.length should be 2");
            assert.isTrue(Object.prototype.__lookupGetter__.length === 1, "__lookupGetter__.length should be 1");
            assert.isTrue(Object.prototype.__lookupSetter__.length === 1, "__lookupSetter__.length should be 1");
        }
    },
    test10: {
        name: "__defineGetter__ and __defineSetter__ should convert null/undefined this argument to global object",
        body: function () {
            Object.prototype.__defineGetter__.call(undefined, "test10_undefined_getter", function () { return undefined; });
            Object.prototype.__defineGetter__.call(null, "test10_null_getter", function () { return undefined; });
            Object.prototype.__defineSetter__.call(undefined, "test10_undefined_setter", function (v) { });
            Object.prototype.__defineSetter__.call(null, "test10_null_setter", function (v) { });

            assert.isTrue(globalObject.hasOwnProperty("test10_undefined_getter"), "global object should now have a getter named test10_undefined_getter");
            assert.isTrue(globalObject.hasOwnProperty("test10_null_getter"), "global object should now have a getter named test10_null_getter");
            assert.isTrue(globalObject.hasOwnProperty("test10_undefined_setter"), "global object should now have a setter named test10_undefined_setter");
            assert.isTrue(globalObject.hasOwnProperty("test10_null_setter"), "global object should now have a setter named test10_null_setter");

            delete globalObject["test10_undefined_getter"];
            delete globalObject["test10_null_getter"];
            delete globalObject["test10_undefined_setter"];
            delete globalObject["test10_null_setter"];
        }
    },
    test11: {
        name: "__lookupGetter__ and __lookupSetter__ find getters and setters of the given name on the calling object respectively",
        body: function () {
            var o = {
                get a() { return undefined; },
                set b(v) { },
            };
            var a = Object.getOwnPropertyDescriptor(o, "a").get;
            var b = Object.getOwnPropertyDescriptor(o, "b").set;

            var f = o.__lookupGetter__("a");

            assert.isTrue(f !== undefined, "__lookupGetter__ should have returned a value");
            assert.isTrue(typeof f === "function", "That value should be a function");
            assert.isTrue(f === a, "And it should be the same function returned by Object.getOwnPropertyDescriptor");

            f = o.__lookupSetter__("b");

            assert.isTrue(f !== undefined, "__lookupSetter__ should have returned a value");
            assert.isTrue(typeof f === "function", "That value should be a function");
            assert.isTrue(f === b, "And it should be the same function returned by Object.getOwnPropertyDescriptor");
        }
    },
    test12: {
        name: "__lookupGetter__ and __lookupSetter__ should look for accessors up the prototype chain",
        body: function () {
            var a = function () { return undefined; };
            var b = function (v) { };

            function Foo () { }
            Object.defineProperty(Foo.prototype, "a", { get: a });
            Object.defineProperty(Foo.prototype, "b", { set: b });

            var o = new Foo();

            var f = o.__lookupGetter__("a");

            assert.isTrue(f !== undefined, "__lookupGetter__ should have returned a value");
            assert.isTrue(typeof f === "function", "That value should be a function");
            assert.isTrue(f === a, "And it should be the same function as the defined getter");

            f = o.__lookupSetter__("b");

            assert.isTrue(f !== undefined, "__lookupSetter__ should have returned a value");
            assert.isTrue(typeof f === "function", "That value should be a function");
            assert.isTrue(f === b, "And it should be the same function as the defined setter");
        }
    },
    test13: {
        name: "__lookupGetter__ and __lookupSetter__ should look for accessors up the prototype chain",
        body: function () {
            var getfn = function () { return undefined; };
            var setfn = function (v) { };

            function Foo () { }
            Object.defineProperty(Foo.prototype, "geta", { get: getfn });
            Object.defineProperty(Foo.prototype, "getb", { get: getfn });
            Object.defineProperty(Foo.prototype, "seta", { set: setfn });
            Object.defineProperty(Foo.prototype, "setb", { set: setfn });

            var o = new Foo();
            Object.defineProperty(o, "geta", { set: setfn, configurable: true, enumerable: true });
            Object.defineProperty(o, "getb", { value: 123, configurable: true, enumerable: true, writable: true });
            Object.defineProperty(o, "seta", { get: getfn, configurable: true, enumerable: true });
            Object.defineProperty(o, "setb", { value: 123, configurable: true, enumerable: true, writable: true });

            WScript.Echo(o.__lookupGetter__("geta"));

            assert.isTrue(o.__lookupGetter__("geta") === undefined, "accessor property on o shadows accessor property on prototype but it is set-only so looking up a getter should return undefined");
            assert.isTrue(o.__lookupGetter__("getb") === getfn, "data property on o shadows accessor property on prototype but __lookupGetter__ looks for the first accessor property, skipping all others, so should return getfn");
            assert.isTrue(o.__lookupSetter__("seta") === undefined, "accessor property on o shadows accessor property on prototype but it is get-only so looking up a setter should return undefined");
            assert.isTrue(o.__lookupSetter__("setb") === setfn, "data property on o shadows accessor property on prototype but __lookupGetter__ looks for the first accessor property, skipping all others, so should return setfn");
        }
    },
};

testRunner.runTests(tests);

