//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Genertors APIs tests -- verifies built-in API objects and properties

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var globObj = this;

function checkAttributes(name, o, p, a) {
    var desc = Object.getOwnPropertyDescriptor(o, p);

    var msgPrefix = "Property " + p.toString() + " on " + name + " is ";

    assert.isTrue(!!desc, msgPrefix + "not found; there is no descriptor");

    assert.areEqual(a.writable, desc.writable, msgPrefix + (a.writable ? "" : "not") + " writable");
    assert.areEqual(a.enumerable, desc.enumerable, msgPrefix + (a.enumerable ? "" : "not") + " enumerable");
    assert.areEqual(a.configurable, desc.configurable, msgPrefix + (a.configurable ? "" : "not") + " configurable");
}

var tests = [
   {
        name: "GeneratorFunction is not exposed on the global object",
        body: function () {
            assert.isFalse(globObj.hasOwnProperty("GeneratorFunction"), "Global object does not have property named GeneratorFunction");
        }
    },
    {
        name: "Generator function object instances have length, name, and prototype properties",
        body: function () {
            function* gf() { }

            assert.isTrue(gf.hasOwnProperty("length"), "Generator function objects have a 'length' property");
            assert.isTrue(gf.hasOwnProperty("name"), "Generator function objects have a 'name' property");
            assert.isTrue(gf.hasOwnProperty("prototype"), "Generator function objects have a 'prototype' property");

            checkAttributes("gf", gf, "length", { writable: false, enumerable: false, configurable: true });
            checkAttributes("gf", gf, "name", { writable: false, enumerable: false, configurable: true });
            checkAttributes("gf", gf, "prototype", { writable: true, enumerable: false, configurable: false });

            function gf2(a, b, c) { }
            assert.areEqual(0, gf.length, "Generator function object's 'length' property matches the number of parameters (0)");
            assert.areEqual(3, gf2.length, "Generator function object's 'length' property matches the number of parameters (3)");
            assert.areEqual("gf", gf.name, "Generator function object's 'name' property matches the function's name");

            assert.isFalse(gf.prototype.hasOwnProperty("constructor"), "Generator function prototype objects do not get a 'constructor' property");
        }
    },
    {
        name: "arguments and caller properties are absent regardless of strictness",
        body: function () {
            function* gf() { }

            assert.isFalse(gf.hasOwnProperty("arguments"), "Generator function objects do not have an 'arguments' property");
            assert.isFalse(gf.hasOwnProperty("caller"), "Generator function objects do not have a 'caller' property");

            // Test JavascriptGeneratorFunction APIs that special case PropertyIds::caller and ::arguments
            Object.setPrototypeOf(gf, Object.prototype); // Remove Function.prototype so we don't find its 'caller' and 'arguments' in these operations
            assert.isFalse("arguments" in gf, "Has operation on 'arguments' property returns false initially");
            assert.areEqual(undefined, gf.arguments, "Get operation on 'arguments' property returns undefined initially");
            assert.areEqual(undefined, Object.getOwnPropertyDescriptor(gf, "arguments"), "No property descriptor for 'arguments' initially");
            assert.isTrue(delete gf.arguments, "Delete operation on 'arguments' property returns true");

            assert.areEqual(0, gf.arguments = 0, "Set operation on 'arguments' creates new property with assigned value");
            assert.isTrue("arguments" in gf, "Has operation on 'arguments' property returns true now");
            assert.areEqual(0, gf.arguments, "Get operation on 'arguments' property returns property value now");
            checkAttributes("gf", gf, "arguments", { writable: true, enumerable: true, configurable: true });
            assert.isTrue(delete gf.arguments, "Delete operation on 'arguments' property still returns true");
            assert.isFalse(gf.hasOwnProperty("arguments"), "'arguments' property is gone");

            assert.isFalse("caller" in gf, "Has operation on 'caller' property returns false initially");
            assert.areEqual(undefined, gf.caller, "Get operation on 'caller' property returns undefined initially");
            assert.areEqual(undefined, Object.getOwnPropertyDescriptor(gf, "caller"), "No property descriptor for 'caller' initially");
            assert.isTrue(delete gf.caller, "Delete operation on 'caller' property returns true");

            assert.areEqual(0, gf.caller = 0, "Set operation on 'caller' creates new property with assigned value");
            assert.isTrue("caller" in gf, "Has operation on 'caller' property returns true now");
            assert.areEqual(0, gf.caller, "Get operation on 'caller' property returns property value now");
            checkAttributes("gf", gf, "caller", { writable: true, enumerable: true, configurable: true });
            assert.isTrue(delete gf.caller, "Delete operation on 'caller' property still returns true");
            assert.isFalse(gf.hasOwnProperty("caller"), "'caller' property is gone");

            function* gfstrict() { "use strict"; }

            assert.isFalse(gfstrict.hasOwnProperty("arguments"), "Strict mode generator function objects do not have an 'arguments' property");
            assert.isFalse(gfstrict.hasOwnProperty("caller"), "Strict mode generator function objects do not have a 'caller' property");

            Object.setPrototypeOf(gfstrict, Object.prototype); // Remove Function.prototype so we don't find its 'caller' and 'arguments' in these operations
            assert.isFalse("arguments" in gfstrict, "Has operation on 'arguments' property returns false initially for a strict mode generator function");
            assert.areEqual(undefined, gfstrict.arguments, "Get operation on 'arguments' property returns undefined initially for a strict mode generator function");
            assert.areEqual(undefined, Object.getOwnPropertyDescriptor(gfstrict, "arguments"), "No property descriptor for 'arguments' initially for a strict mode generator function");
            assert.isTrue(delete gfstrict.arguments, "Delete operation on 'arguments' property returns true initially for a strict mode generator function");

            assert.areEqual(0, gfstrict.arguments = 0, "Set operation on 'arguments' creates new property with assigned value for a strict mode generator function");
            assert.isTrue("arguments" in gfstrict, "Has operation on 'arguments' property returns true now for a strict mode generator function");
            assert.areEqual(0, gfstrict.arguments, "Get operation on 'arguments' property returns property value now for a strict mode generator function");
            checkAttributes("gfstrict", gfstrict, "arguments", { writable: true, enumerable: true, configurable: true });
            assert.isTrue(delete gfstrict.arguments, "Delete operation on 'arguments' property still returns true for a strict mode generator function");
            assert.isFalse(gfstrict.hasOwnProperty("arguments"), "'arguments' property is gone for a strict mode generator function");

            assert.isFalse("caller" in gfstrict, "Has operation on 'caller' property returns false initially for a strict mode generator function");
            assert.areEqual(undefined, gfstrict.caller, "Get operation on 'caller' property returns undefined initially for a strict mode generator function");
            assert.areEqual(undefined, Object.getOwnPropertyDescriptor(gfstrict, "caller"), "No property descriptor for 'caller' initially for a strict mode generator function");
            assert.isTrue(delete gfstrict.caller, "Delete operation on 'caller' property returns true initially for a strict mode generator function");

            assert.areEqual(0, gfstrict.caller = 0, "Set operation on 'caller' creates new property with assigned value for a strict mode generator function");
            assert.isTrue("caller" in gfstrict, "Has operation on 'caller' property returns true now for a strict mode generator function");
            assert.areEqual(0, gfstrict.caller, "Get operation on 'caller' property returns property value now for a strict mode generator function");
            checkAttributes("gfstrict", gfstrict, "caller", { writable: true, enumerable: true, configurable: true });
            assert.isTrue(delete gfstrict.caller, "Delete operation on 'caller' property still returns true for a strict mode generator function");
            assert.isFalse(gfstrict.hasOwnProperty("caller"), "'caller' property is gone for a strict mode generator function");

        }
    },
    {
        name: "Generator functions' length property is the number of formal parameters",
        body: function () {
            function* gf0() { }
            function* gf1(a) { }
            function* gf5(a,b,c,d,e) { }

            assert.areEqual(0, gf0.length, "Generator function with no formal parameters has length 0");
            assert.areEqual(1, gf1.length, "Generator function with one formal parameter has length 1");
            assert.areEqual(5, gf5.length, "Generator function with five formal parameters has length 5");
        }
    },
    {
        name: "Generator function instances have GeneratorFunction.prototype as their prototype and it has the specified properties and prototype",
        body: function () {
            function* gf() { }
            var generatorFunctionPrototype = Object.getPrototypeOf(gf);

            assert.areEqual(Function.prototype, Object.getPrototypeOf(generatorFunctionPrototype), "GeneratorFunction.prototype's prototype is Function.prototype");

            assert.isTrue(generatorFunctionPrototype.hasOwnProperty("constructor"), "GeneratorFunction.prototype has 'constructor' property");
            assert.isTrue(generatorFunctionPrototype.hasOwnProperty("prototype"), "GeneratorFunction.prototype has 'prototype' property");
            assert.isTrue(generatorFunctionPrototype.hasOwnProperty(Symbol.toStringTag), "GeneratorFunction.prototype has [Symbol.toStringTag] property");

            checkAttributes("GeneratorFunction.prototype", generatorFunctionPrototype, "constructor", { writable: false, enumerable: false, configurable: true });
            checkAttributes("GeneratorFunction.prototype", generatorFunctionPrototype, "prototype", { writable: false, enumerable: false, configurable: true });
            checkAttributes("GeneratorFunction.prototype", generatorFunctionPrototype, Symbol.toStringTag, { writable: false, enumerable: false, configurable: true });

            assert.areEqual("GeneratorFunction", generatorFunctionPrototype[Symbol.toStringTag], "GeneratorFunction.prototype's [Symbol.toStringTag] property is 'GeneratorFunction'");
        }
    },
    {
        name: "GeneratorFunction constructor is the value of the constructor property of GeneratorFunction.prototype and has the specified properties and prototype",
        body: function () {
            function* gf() { }
            var generatorFunctionPrototype = Object.getPrototypeOf(gf);
            var generatorFunctionConstructor = generatorFunctionPrototype.constructor;

            assert.areEqual(Function, Object.getPrototypeOf(generatorFunctionConstructor), "GeneratorFunction's prototype is Function");

            assert.isTrue(generatorFunctionConstructor.hasOwnProperty("length"), "GeneratorFunction has 'length' property");
            assert.isTrue(generatorFunctionConstructor.hasOwnProperty("prototype"), "GeneratorFunction has 'prototype' property");

            checkAttributes("GeneratorFunction", generatorFunctionConstructor, "length", { writable: false, enumerable: false, configurable: true });
            checkAttributes("GeneratorFunction", generatorFunctionConstructor, "prototype", { writable: false, enumerable: false, configurable: false });

            assert.areEqual(generatorFunctionPrototype, generatorFunctionConstructor.prototype, "GeneratorFunction's 'prototype' property is GeneratorFunction.prototype");
            assert.areEqual(1, generatorFunctionConstructor.length, "GeneratorFunction's 'length' property is 1");
        }
    },
    {
        name: "Generator prototype is the value of the prototype property of GeneratorFunction.prototype and has the specified properties and prototype",
        body: function () {
            function* gf() { }
            var generatorFunctionPrototype = Object.getPrototypeOf(gf);
            var generatorPrototype = generatorFunctionPrototype.prototype;
            var iteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]()));

            assert.areEqual(iteratorPrototype, Object.getPrototypeOf(generatorPrototype), "Generator prototype's prototype is %IteratorPrototype%");

            assert.isTrue(generatorPrototype.hasOwnProperty("constructor"), "Generator prototype has 'constructor' property");
            assert.isTrue(generatorPrototype.hasOwnProperty("next"), "Generator prototype has 'next' property");
            assert.isTrue(generatorPrototype.hasOwnProperty("throw"), "Generator prototype has 'throw' property");
            assert.isTrue(generatorPrototype.hasOwnProperty("return"), "Generator prototype has 'return' property");
            assert.isFalse(generatorPrototype.hasOwnProperty(Symbol.iterator), "Generator prototype does not have [Symbol.iterator] property");
            assert.isTrue(generatorPrototype.hasOwnProperty(Symbol.toStringTag), "Generator prototype has [Symbol.toStringTag] property");

            checkAttributes("Generator prototype", generatorPrototype, "constructor", { writable: false, enumerable: false, configurable: true });
            checkAttributes("Generator prototype", generatorPrototype, "next", { writable: true, enumerable: false, configurable: true });
            checkAttributes("Generator prototype", generatorPrototype, "return", { writable: true, enumerable: false, configurable: true });
            checkAttributes("Generator prototype", generatorPrototype, "throw", { writable: true, enumerable: false, configurable: true });
            checkAttributes("Generator prototype", generatorPrototype, Symbol.toStringTag, { writable: false, enumerable: false, configurable: true });

            assert.areEqual(generatorFunctionPrototype, generatorPrototype.constructor, "Generator prototype's 'constructor' property is GeneratorFunction.prototype");
            assert.areEqual("Generator", generatorPrototype[Symbol.toStringTag], "Generator prototype's [Symbol.toStringTag] property is 'Generator'");
        }
    },
    {
        name: "Generator object prototype by default is the function's .prototype property value whose prototype is the Generator prototype",
        body: function () {
            function* gf() { }
            var generatorFunctionPrototype = Object.getPrototypeOf(gf);
            var generatorPrototype = generatorFunctionPrototype.prototype;

            var g = gf();
            assert.areEqual(generatorPrototype, Object.getPrototypeOf(Object.getPrototypeOf(g)), "Generator object's prototype is an object whose prototype is Generator prototype");
            assert.areEqual(Object.getPrototypeOf(g), gf.prototype, "Generator object's prototype comes from generator function's .prototype property");
            assert.isTrue(g instanceof gf, "Generator object is instance of the generator function");
        }
    },
    {
        name: "Generator function's arguments.callee should be equal to the generator function object itself",
        body: function () {
            function* gf() {
                assert.isTrue(arguments.callee === gf, "arguments.callee should be the same function object pointed to by gf");
            }

            gf().next();
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
