//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 restricted property tests

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function verifyAttributes(obj, prop, attribs, name) {
    var p = Object.getOwnPropertyDescriptor(obj, prop);

    assert.areNotEqual(undefined, p, name + " does not have property named " + prop);

    assert.areEqual(attribs.writable, p.writable, name + " has property named " + prop + " with writable = " + attribs.writable);
    assert.areEqual(attribs.enumerable, p.enumerable, name + " has property named " + prop + " with enumerable = " + attribs.enumerable);
    assert.areEqual(attribs.configurable, p.configurable, name + " has property named " + prop + " with configurable = " + attribs.configurable);
}

function verifyHasRestrictedOwnProperties(obj, name) {
    assert.isTrue(obj.hasOwnProperty('caller'), name + " reports that it has own property 'caller'")
    assert.isTrue(obj.hasOwnProperty('arguments'), name + " reports that it has own property 'arguments'")

    var names = Object.getOwnPropertyNames(obj);
    assert.areNotEqual(-1, names.findIndex((e) => { return e === 'arguments'; }), name + " has 'arguments' own property");
    assert.areNotEqual(-1, names.findIndex((e) => { return e === 'caller'; }), name + " has 'caller' own property");

    verifyAttributes(obj, 'caller', { writable: false, enumerable: false, configurable: false }, name);
    assert.isFalse(obj.propertyIsEnumerable('caller'), name + " says 'caller' property is not enumerable");
    verifyAttributes(obj, 'arguments', { writable: false, enumerable: false, configurable: false }, name);
    assert.isFalse(obj.propertyIsEnumerable('arguments'), name + " says 'arguments' property is not enumerable");

    assert.areEqual(null, obj.caller, name + " says 'caller' property is null")
    assert.areEqual(null, obj.arguments, name + " says 'arguments' property is null")

    assert.doesNotThrow(function() { obj.caller = 'something'; }, name + " has 'caller' property which can't be assigned to");
    assert.doesNotThrow(function() { obj.arguments = 'something'; }, name + " has 'arguments' property which  can't be assigned to");

    assert.throws(function() { 'use strict'; obj.caller = 'something'; }, TypeError, name + " has 'caller' own property but it is not configurable so we will throw in strict mode", "Assignment to read-only properties is not allowed in strict mode");
    assert.throws(function() { 'use strict'; obj.arguments = 'something'; }, TypeError, name + " has 'arguments' own property but it is not configurable so we will throw in strict mode", "Assignment to read-only properties is not allowed in strict mode");

    assert.areEqual(null, obj.caller, name + " says 'caller' property is null")
    assert.areEqual(null, obj.arguments, name + " says 'arguments' property is null")

    assert.throws(function() { Object.defineProperty(obj, 'arguments', { value: 123 }); }, TypeError, name + " has 'arguments' property as non-writable, non-configurable", "Cannot modify non-writable property 'arguments'");
    assert.throws(function() { Object.defineProperty(obj, 'caller', { value: 123 }); }, TypeError, name + " has 'caller' property as non-writable, non-configurable", "Cannot modify non-writable property 'caller'");

    assert.isFalse(delete obj.arguments, name + " has 'arguments' property as non-configurable so delete returns false");
    assert.isFalse(delete obj.caller, name + " has 'caller' property as non-configurable so delete returns false");

    assert.throws(function() { 'use strict'; delete obj.caller; }, TypeError, name + " has 'caller' own property but it is not configurable so we will throw in strict mode", "Calling delete on 'caller' is not allowed in strict mode");
    assert.throws(function() { 'use strict'; delete obj.arguments; }, TypeError, name + " has 'arguments' own property but it is not configurable so we will throw in strict mode", "Calling delete on 'arguments' is not allowed in strict mode");
}

function verifyDoesNotHaveRestrictedOwnProperties(obj, name) {
    assert.isFalse(obj.hasOwnProperty('caller'), name + " does not report that it has own property 'caller'")
    assert.isFalse(obj.hasOwnProperty('arguments'), name + " does not report that it has own property 'arguments'")

    var names = Object.getOwnPropertyNames(obj);
    assert.areEqual(-1, names.findIndex((e) => { return e === 'arguments'; }), name + " does not have 'arguments' own property");
    assert.areEqual(-1, names.findIndex((e) => { return e === 'caller'; }), name + " does not have 'caller' own property");

    assert.areEqual(undefined, Object.getOwnPropertyDescriptor(obj, 'caller'), name + " does not have 'caller' own property")
    assert.isFalse(obj.propertyIsEnumerable('caller'), name + " says 'caller' property is not enumerable");
    assert.areEqual(undefined, Object.getOwnPropertyDescriptor(obj, 'arguments'), name + " does not have 'arguments' own property");
    assert.isFalse(obj.propertyIsEnumerable('arguments'), name + " says 'arguments' property is not enumerable");

    assert.throws(function() { obj.caller; }, TypeError, name + " throws on access to 'caller' property", "Accessing the 'caller' property is restricted in this context");
    assert.throws(function() { obj.arguments; }, TypeError, name + " throws on access to 'arguments' property", "Accessing the 'arguments' property is restricted in this context");

    assert.throws(function() { 'use strict'; obj.caller; }, TypeError, name + " throws on access to 'caller' property in strict mode", "Accessing the 'caller' property is restricted in this context");
    assert.throws(function() { 'use strict'; obj.arguments; }, TypeError, name + " throws on access to 'arguments' property in strict mode", "Accessing the 'arguments' property is restricted in this context");

    assert.throws(function() { obj.caller = 'something'; }, TypeError, name + " throws trying to assign to 'caller' property", "Accessing the 'caller' property is restricted in this context");
    assert.throws(function() { obj.arguments = 'something'; }, TypeError, name + " throws trying to assign to 'arguments' property", "Accessing the 'arguments' property is restricted in this context");

    assert.throws(function() { 'use strict'; obj.caller = 'something'; }, TypeError, name + " throws trying to assign to 'caller' property in strict mode", "Accessing the 'caller' property is restricted in this context");
    assert.throws(function() { 'use strict'; obj.arguments = 'something'; }, TypeError, name + " throws trying to assign to 'arguments' property in strict mode", "Accessing the 'arguments' property is restricted in this context");

    assert.isTrue(delete obj.arguments, name + " allows deleting own property named 'arguments' if that property doesn't exist");
    assert.doesNotThrow(function() { Object.defineProperty(obj, 'arguments', { value: 123, writable: true, enumerable: true, configurable: true }); }, name + " doesn't have own 'arguments' property");
    assert.isTrue(obj.hasOwnProperty('arguments'), name + " has own property 'arguments' after defineProperty")
    assert.isTrue(obj.propertyIsEnumerable('arguments'), name + " says 'arguments' property is enumerable if it is an enumerable own property");
    assert.areEqual(123, obj.arguments, name + " can have an own property defined for 'arguments'")
    verifyAttributes(obj, 'arguments', { writable: true, enumerable: true, configurable: true }, name);
    assert.isTrue(delete obj.arguments, name + " allows deleting own property named 'arguments' if that property does exist");
    assert.isFalse(obj.hasOwnProperty('arguments'), name + " doesn't have own property 'arguments' after delete")

    assert.isTrue(delete obj.caller, name + " allows deleting own property named 'caller' if that property doesn't exist");
    assert.doesNotThrow(function() { Object.defineProperty(obj, 'caller', { value: 123, writable: true, enumerable: true, configurable: true }); }, name + " doesn't have own 'caller' property");
    assert.isTrue(obj.hasOwnProperty('caller'), name + " has own property 'caller' after defineProperty")
    assert.isTrue(obj.propertyIsEnumerable('caller'), name + " says 'caller' property is enumerable if it is an enumerable own property");
    assert.areEqual(123, obj.caller, name + " can have an own property defined for 'caller'")
    verifyAttributes(obj, 'caller', { writable: true, enumerable: true, configurable: true }, name);
    assert.isTrue(delete obj.caller, name + " allows deleting own property named 'caller' if that property does exist");
    assert.isFalse(obj.hasOwnProperty('caller'), name + " doesn't have own property 'caller' after delete")

    // Remove Function.prototype from the prototype chain.
    Object.setPrototypeOf(obj, Object.prototype);
    assert.areEqual(undefined, obj.arguments, name + " does not initially have 'arguments' property when disconnected from Function.prototype");
    assert.doesNotThrow(function() { obj.arguments = 'abc'; }, name + " can set the 'arguments' property when disconnected from Function.prototype");
    assert.areEqual('abc', obj.arguments, name + " can set the 'arguments' property when disconnected from Function.prototype");
    assert.isTrue(obj.hasOwnProperty('arguments'), name + " has 'arguments' own property")
    assert.isTrue(obj.propertyIsEnumerable('arguments'), name + " says 'arguments' property is enumerable if it is an enumerable own property");
    verifyAttributes(obj, 'arguments', { writable: true, enumerable: true, configurable: true }, name);
    assert.isTrue(delete obj.arguments, name + " allows deleting own property named 'arguments' if that property does exist");
    assert.isFalse(obj.hasOwnProperty('arguments'), name + " doesn't have own property 'arguments' after delete")

    assert.areEqual(undefined, obj.caller, name + " does not initially have 'caller' property when disconnected from Function.prototype");
    assert.doesNotThrow(function() { obj.caller = 'abc'; }, name + " can set the 'caller' property when disconnected from Function.prototype");
    assert.areEqual('abc', obj.caller, name + " can set the 'caller' property when disconnected from Function.prototype");
    assert.isTrue(obj.hasOwnProperty('caller'), name + " has 'caller' own property")
    assert.isTrue(obj.propertyIsEnumerable('caller'), name + " says 'caller' property is enumerable if it is an enumerable own property");
    verifyAttributes(obj, 'caller', { writable: true, enumerable: true, configurable: true }, name);
    assert.isTrue(delete obj.caller, name + " allows deleting own property named 'caller' if that property does exist");
    assert.isFalse(obj.hasOwnProperty('caller'), name + " doesn't have own property 'caller' after delete")
}

var tests = [
    {
        name: "Restricted properties of Function.prototype",
        body: function () {
            var obj = Function.prototype;

            assert.isTrue(obj.hasOwnProperty('caller'), "Function.prototype has own property 'caller'")
            assert.isTrue(obj.hasOwnProperty('arguments'), "Function.prototype has own property 'arguments'")

            var p = Object.getOwnPropertyDescriptor(obj, 'caller');
            assert.isFalse(p.enumerable, "Function.prototype function has 'caller' own property which is not enumerable");
            assert.isFalse(p.configurable, "Function.prototype function has 'caller' own property which is not configurable");
            assert.isFalse(obj.propertyIsEnumerable('caller'), "Function.prototype says 'caller' property is not enumerable");
            assert.areEqual('function', typeof p.get, "Function.prototype['caller'] has get accessor function");
            assert.areEqual('function', typeof p.set, "Function.prototype['caller'] has set accessor function");
            assert.throws(function() { p.get(); }, TypeError, "Function.prototype['caller'] has get accessor which throws");
            assert.throws(function() { p.set(); }, TypeError, "Function.prototype['caller'] has set accessor which throws");
            assert.isTrue(p.get === p.set, "Function.prototype returns the same ThrowTypeError function for get/set accessor of 'caller' property");

            var p2 = Object.getOwnPropertyDescriptor(obj, 'arguments');
            assert.isFalse(p2.enumerable, "Function.prototype function has 'arguments' own property which is not enumerable");
            assert.isFalse(p2.configurable, "Function.prototype function has 'arguments' own property which is not configurable");
            assert.isFalse(obj.propertyIsEnumerable('arguments'), "Function.prototype says 'arguments' property is not enumerable");
            assert.areEqual('function', typeof p2.get, "Function.prototype['arguments'] has get accessor function");
            assert.areEqual('function', typeof p2.set, "Function.prototype['arguments'] has set accessor function");
            assert.throws(function() { p2.get(); }, TypeError, "Function.prototype['arguments'] has get accessor which throws");
            assert.throws(function() { p2.set(); }, TypeError, "Function.prototype['arguments'] has set accessor which throws");
            assert.isTrue(p2.get === p2.set, "Function.prototype returns the same ThrowTypeError function for get/set accessor of 'arguments' property");

            assert.isTrue(p.get === p2.get, "Function.prototype returns the same ThrowTypeError function for accessor of both 'arguments' and 'caller' properties");

            var names = Object.getOwnPropertyNames(obj);
            assert.areNotEqual(-1, names.findIndex((e) => { return e === 'arguments'; }), "Function.prototype has 'arguments' own property");
            assert.areNotEqual(-1, names.findIndex((e) => { return e === 'caller'; }), "Function.prototype has 'caller' own property");

            assert.throws(function() { obj.caller; }, TypeError, "Function.prototype throws on access to 'caller' property", "Accessing the 'caller' property is restricted in this context");
            assert.throws(function() { obj.arguments; }, TypeError, "Function.prototype throws on access to 'arguments' property", "Accessing the 'arguments' property is restricted in this context");

            assert.throws(function() { obj.caller = 'something'; }, TypeError, "Function.prototype throws trying to assign to 'caller' property", "Accessing the 'caller' property is restricted in this context");
            assert.throws(function() { obj.arguments = 'something'; }, TypeError, "Function.prototype throws trying to assign to 'arguments' property", "Accessing the 'arguments' property is restricted in this context");

            // TODO: These descriptors should have configurable set to true so remaining asserts in this test should actually succeed
            assert.throws(function() { Object.defineProperty(obj, 'arguments', { value: 123 }); }, TypeError, "Function.prototype has 'arguments' property as non-configurable", "Cannot redefine non-configurable property 'arguments'");
            assert.throws(function() { Object.defineProperty(obj, 'caller', { value: 123 }); }, TypeError, "Function.prototype has 'caller' property as non-configurable", "Cannot redefine non-configurable property 'caller'");

            assert.isFalse(delete obj.arguments, "Function.prototype has 'arguments' property as non-configurable so delete returns false");
            assert.isFalse(delete obj.caller, "Function.prototype has 'caller' property as non-configurable so delete returns false");

            assert.throws(function() { 'use strict'; delete obj.caller; }, TypeError, "Function.prototype has 'caller' own property but it is not configurable so we will throw in strict mode", "Calling delete on 'caller' is not allowed in strict mode");
            assert.throws(function() { 'use strict'; delete obj.arguments; }, TypeError, "Function.prototype has 'arguments' own property but it is not configurable so we will throw in strict mode", "Calling delete on 'arguments' is not allowed in strict mode");
        }
    },
    {
        name: "Restricted properties of non-strict function",
        body: function () {
            function obj() {};

            verifyHasRestrictedOwnProperties(obj, "Non-strict function");
        }
    },
    {
        name: "Restricted properties of strict function",
        body: function () {
            function foo() { 'use strict'; };

            verifyDoesNotHaveRestrictedOwnProperties(foo, "Strict function");
        }
    },
    {
        name: "Restricted properties of class",
        body: function () {
            class A { };

            verifyDoesNotHaveRestrictedOwnProperties(A, "Class");
        }
    },
    {
        name: "Restricted properties of class static method",
        body: function () {
            class A {
                static static_method() { }
            };

            verifyDoesNotHaveRestrictedOwnProperties(A.static_method, "Class static method");
        }
    },
    {
        name: "Restricted properties of strict-mode class static method",
        body: function () {
            class A {
                static static_method() { 'use strict'; }
            };

            verifyDoesNotHaveRestrictedOwnProperties(A.static_method, "Class strict-mode static method");
        }
    },
    {
        name: "Restricted properties of class method",
        body: function () {
            class A {
                method() { }
            };

            verifyDoesNotHaveRestrictedOwnProperties(A.prototype.method, "Class method");
        }
    },
    {
        name: "Restricted properties of strict-mode class method",
        body: function () {
            class A {
                method() { 'use strict'; }
            };

            verifyDoesNotHaveRestrictedOwnProperties(A.prototype.method, "Class strict-mode method");
        }
    },
    {
        name: "Restricted properties of class with 'caller' static method",
        body: function () {
            var obj = class A {
                static caller() { return 42; }
            };

            assert.isTrue(obj.hasOwnProperty('caller'), "Class does has own property 'caller'")
            assert.isFalse(obj.hasOwnProperty('arguments'), "Class does not report that it has own property 'arguments'")

            assert.areEqual('{"writable":true,"enumerable":false,"configurable":true}', JSON.stringify(Object.getOwnPropertyDescriptor(obj, 'caller')), "Class does not have 'caller' own property")
            assert.areEqual(undefined, JSON.stringify(Object.getOwnPropertyDescriptor(obj, 'arguments')), "Class does not have 'arguments' own property");
            assert.areEqual('["caller","length","name","prototype"]', JSON.stringify(Object.getOwnPropertyNames(obj).sort()), "Class does not have 'caller' and 'arguments' own properties");

            assert.areEqual(42, obj.caller(), "Accessing the 'caller' property is not restricted");
            assert.throws(function() { obj.arguments; }, TypeError, "Class throws on access to 'arguments' property", "Accessing the 'arguments' property is restricted in this context");
        }
    },
    {
        name: "Restricted properties of class with 'arguments' static get method",
        body: function () {
            var obj = class A {
                static get arguments() { return 42; }
            };

            assert.isFalse(obj.hasOwnProperty('caller'), "Class does not report that it has own property 'caller'")
            assert.isTrue(obj.hasOwnProperty('arguments'), "Class has own property 'arguments'")

            assert.areEqual(undefined, JSON.stringify(Object.getOwnPropertyDescriptor(obj, 'caller')), "Class does not have 'caller' own property")
            assert.areEqual('{"enumerable":false,"configurable":true}', JSON.stringify(Object.getOwnPropertyDescriptor(obj, 'arguments')), "Class has 'arguments' own property");
            assert.areEqual('["arguments","length","name","prototype"]', JSON.stringify(Object.getOwnPropertyNames(obj).sort()), "Class has 'arguments' own property, no 'caller' own property");

            assert.throws(function() { obj.caller; }, TypeError, "Class method throws on access to 'caller' property", "Accessing the 'caller' property is restricted in this context");
            assert.areEqual(42, obj.arguments, "Accessing the 'arguments' property is not restricted");
        }
    },
    {
        name: "Restricted properties of class with 'arguments' set method",
        body: function () {
            var my_v;
            class A {
                set arguments(v) { my_v = v; }
            };
            var obj = A;

            assert.isFalse(obj.hasOwnProperty('caller'), "Class does not report that it has own property 'caller'")
            assert.isFalse(obj.hasOwnProperty('arguments'), "Class does not report that it has own property 'arguments'")

            assert.areEqual(undefined, JSON.stringify(Object.getOwnPropertyDescriptor(obj, 'caller')), "Class does not have 'caller' own property")
            assert.areEqual(undefined, JSON.stringify(Object.getOwnPropertyDescriptor(obj, 'arguments')), "Class has 'arguments' own property");
            assert.areEqual('["length","name","prototype"]', JSON.stringify(Object.getOwnPropertyNames(obj).sort()), "Class has 'arguments' own property, no 'caller' own property");

            assert.throws(function() { obj.caller; }, TypeError, "Class method throws on access to 'caller' property", "Accessing the 'caller' property is restricted in this context");
            assert.throws(function() { obj.arguments; }, TypeError, "Class method throws on access to 'arguments' property", "Accessing the 'arguments' property is restricted in this context");

            var a = new A();

            assert.isFalse(a.hasOwnProperty('caller'), "Class instance does not report that it has own property 'caller'")
            assert.isFalse(a.hasOwnProperty('arguments'), "Class instance does not report that it has own property 'arguments'")

            assert.isFalse(a.__proto__.hasOwnProperty('caller'), "Class prototype does not report that it has own property 'caller'")
            assert.isTrue(a.__proto__.hasOwnProperty('arguments'), "Class prototype has own property 'arguments'")

            a.arguments = 50;
            assert.areEqual(50, my_v, "Accessing the 'arguments' property was not restricted");

            assert.areEqual(undefined, a.caller, "Access to class instance 'caller' property doesn't throw - instance is not a function");

            a.caller = 123;

            assert.areEqual(123, a.caller, "Assignment to class instance 'caller' property works normally");
        }
    },
    {
        name: "Restricted properties of lambda",
        body: function () {
            var obj = () => { }

            verifyDoesNotHaveRestrictedOwnProperties(obj, "Lambda");
        }
    },
    {
        name: "Restricted properties of strict-mode lambda",
        body: function () {
            var obj = () => { 'use strict'; }

            verifyDoesNotHaveRestrictedOwnProperties(obj, "Strict-mode lambda");
        }
    },
    {
        name: "Restricted properties of bound function",
        body: function () {
            function target() {}
            var obj = target.bind(null);

            verifyDoesNotHaveRestrictedOwnProperties(obj, "Bound function");
        }
    },
    {
        name: "Restricted properties of bound strict-mode function",
        body: function () {
            function target() { 'use strict'; }
            var obj = target.bind(null);

            verifyDoesNotHaveRestrictedOwnProperties(obj, "Bound strict-mode function");
        }
    },
    {
        name: "Restricted properties of generator function",
        body: function () {
            function* gf() { }

            verifyDoesNotHaveRestrictedOwnProperties(gf, "Generator function");
        }
    },
    {
        name: "Restricted properties of strict-mode generator function",
        body: function () {
            function* gf() { 'use strict'; }

            verifyDoesNotHaveRestrictedOwnProperties(gf, "Generator strict-mode function");
        }
    },
    {
        name: "Restricted properties of object-literal function",
        body: function () {
            var obj = { func() { } }

            verifyHasRestrictedOwnProperties(obj.func, "Object-literal function");
        }
    },
    {
        name: "Restricted properties of strict-mode object-literal function",
        body: function () {
            var obj = { func() { 'use strict'; } }

            verifyDoesNotHaveRestrictedOwnProperties(obj.func, "Object-literal strict-mode function");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
