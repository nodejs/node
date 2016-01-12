//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

//TODO remove -nonative & exclude_dynapogo when Ian has generator native code support & Taylor has subclassable native support remove -nonnative and native code test exclusions
var tests = [
   {
       name: "tag string check for typed arrays, iterators, generators, and Promise",
       body: function () {
            assert.areEqual("Symbol", Symbol.prototype [Symbol.toStringTag], "check String is Symbol");
            var o  = Object.getOwnPropertyDescriptor(Symbol.prototype, Symbol.toStringTag);
            assert.isFalse(o.writable,     "Symbol @@toStringTag  is not writable");
            assert.isFalse(o.enumerable,   "Symbol @@toStringTag is not enumerable");
            assert.isTrue(o.configurable,  "Symbol @@toStringTag is configurable");

            var o  = Object.getOwnPropertyDescriptor(Symbol, "toStringTag");
            assert.isFalse(o.writable,     "@@toStringTag is not writable");
            assert.isFalse(o.enumerable,   "@@toStringTag is not enumerable");
            assert.isFalse(o.configurable, "@@toStringTag is not configurable");

            assert.areEqual("String Iterator", ""[Symbol.iterator]()[Symbol.toStringTag], "check String is String Iterator");
            assert.areEqual("Array Iterator", [][Symbol.iterator]()[Symbol.toStringTag], "check String is Array Iterator");
            assert.areEqual("Map Iterator", (new Map())[Symbol.iterator]()[Symbol.toStringTag], "check String is Map Iterator");
            assert.areEqual("Set Iterator", (new Set())[Symbol.iterator]()[Symbol.toStringTag], "check String is Set Iterator");

            function* gf() { };
            assert.areEqual("GeneratorFunction", gf[Symbol.toStringTag], "check String is GeneratorFunction");
            var o  = Object.getOwnPropertyDescriptor(gf.__proto__, Symbol.toStringTag);
            assert.isFalse(o.writable,     "Generator function @@toStringTag is not writable");
            assert.isFalse(o.enumerable,   "Generator function @@toStringTag is not enumerable");
            assert.isTrue(o.configurable,  "Generator function @@toStringTag is configurable");

            var gen = gf();
            assert.areEqual("Generator", gen[Symbol.toStringTag], "check String is Generator");
            var o  = Object.getOwnPropertyDescriptor(gen.__proto__.__proto__, Symbol.toStringTag);
            assert.isFalse(o.writable,     "Generator @@toStringTag is not writable");
            assert.isFalse(o.enumerable,   "Generator @@toStringTag is not enumerable");
            assert.isTrue(o.configurable,  "Generator @@toStringTag is not configurable");
            assert.areEqual("Generator",  o.value, "Should be the Same as gen[Symbol.toStringTag], ie Generator");

            assert.areEqual("Promise", Promise.prototype[Symbol.toStringTag], "check String is Promise");
            var o  = Object.getOwnPropertyDescriptor(Promise.prototype, Symbol.toStringTag);
            assert.isFalse(o.writable,     "Promise @@toStringTag is not writable");
            assert.isFalse(o.enumerable,   "Promise @@toStringTag is not enumerable");
            assert.isTrue(o.configurable,  "Promise @@toStringTag is configurable");
       }
    },
    {
       name: "tag string check for Everything else",
       body: function () {
            assert.areEqual("Math", Math[Symbol.toStringTag], "check String is Math");
            var o  = Object.getOwnPropertyDescriptor(Math, Symbol.toStringTag);
            assert.isFalse(o.writable,     "Math @@toStringTag is not writable");
            assert.isFalse(o.enumerable,   "Math @@toStringTag is not enumerable");
            assert.isTrue(o.configurable,  "Math @@toStringTag is configurable");

            assert.areEqual("JSON", JSON[Symbol.toStringTag], "check String is JSON");
            var o  = Object.getOwnPropertyDescriptor(JSON, Symbol.toStringTag);
            assert.isFalse(o.writable,     "JSON @@toStringTag is not writable");
            assert.isFalse(o.enumerable,   "JSON @@toStringTag is not enumerable");
            assert.isTrue(o.configurable,  "JSON @@toStringTag is configurable");

            assert.areEqual("Map", (new Map())[Symbol.toStringTag], "check String is Map");
            var o  = Object.getOwnPropertyDescriptor(Map.prototype, Symbol.toStringTag);
            assert.isFalse(o.writable,     "Map @@toStringTag is not writable");
            assert.isFalse(o.enumerable,   "Map @@toStringTag is not enumerable");
            assert.isTrue(o.configurable,  "Map @@toStringTag is configurable");

            assert.areEqual("WeakMap", (new WeakMap())[Symbol.toStringTag], "check String is WeakMap");
            var o  = Object.getOwnPropertyDescriptor(WeakMap.prototype, Symbol.toStringTag);
            assert.isFalse(o.writable,     "WeakMap @@toStringTag is not writable");
            assert.isFalse(o.enumerable,   "WeakMap @@toStringTag is not enumerable");
            assert.isTrue(o.configurable,  "WeakMap @@toStringTag is configurable");

            assert.areEqual("Set", (new Set())[Symbol.toStringTag], "check String is Set");
            var o  = Object.getOwnPropertyDescriptor(Set.prototype, Symbol.toStringTag);
            assert.isFalse(o.writable,     "Set @@toStringTag is not writable");
            assert.isFalse(o.enumerable,   "Set @@toStringTag is not enumerable");
            assert.isTrue(o.configurable,  "Set @@toStringTag is configurable");

            assert.areEqual("WeakSet", (new WeakSet())[Symbol.toStringTag], "check String is WeakSet");
            var o  = Object.getOwnPropertyDescriptor(WeakSet.prototype, Symbol.toStringTag);
            assert.isFalse(o.writable,     "WeakSet @@toStringTag is not writable");
            assert.isFalse(o.enumerable,   "WeakSet @@toStringTag is not enumerable");
            assert.isTrue(o.configurable,  "WeakSet @@toStringTag is configurable");

            assert.areEqual("ArrayBuffer", (new ArrayBuffer())[Symbol.toStringTag], "check String is ArrayBuffer");
            assert.areEqual("DataView", (new DataView(new ArrayBuffer()))[Symbol.toStringTag], "check String is DataView");
       }
    },
    {
       name: "toString test for Symbol, Generators, Promises, Sets, & maps",
       body: function () {
            assert.areEqual("[object DataView]", Object.prototype.toString.call(new DataView((new ArrayBuffer()))), "toString should have tag DataView");
            assert.areEqual("[object ArrayBuffer]", Object.prototype.toString.call(new ArrayBuffer()), "toString should have tag ArrayBuffer");
            assert.areEqual("[object WeakSet]", Object.prototype.toString.call(new WeakSet()), "toString should have tag WeakSet");
            assert.areEqual("[object Set]", Object.prototype.toString.call(new Set()), "toString should have tag Set");
            assert.areEqual("[object WeakMap]", Object.prototype.toString.call(new WeakMap()), "toString should have tag WeakMap");
            assert.areEqual("[object JSON]", Object.prototype.toString.call(JSON), "toString should have tag JSON");
            assert.areEqual("[object Math]", Object.prototype.toString.call(Math), "toString should have tag Math");
            //assert.areEqual("[object Object]", Object.prototype.toString.call(new Proxy(new Array(), {})), "Proxy toString should have tag Object"); //TODO enable when Taylor fixes Proxy subclassable bug

            function* gf() { };
            var gen = gf();
            assert.areEqual("[object ~GeneratorFunction]", Object.prototype.toString.call(gf), "toString should have tag ~GeneratorFunction b/c it is a subset of Function");
            assert.areEqual("[object Generator]", Object.prototype.toString.call(gen), "toString should have tag Generator");
            assert.areEqual("[object Promise]", Object.prototype.toString.call(new Promise(() => {})), "toString should have tag Promise");
            assert.areEqual("[object String Iterator]", Object.prototype.toString.call(""[Symbol.iterator]()), "toString should have tag String Iterator");
            assert.areEqual("[object Array Iterator]", Object.prototype.toString.call([][Symbol.iterator]()), "toString should have tag Array Iterator");
            assert.areEqual("[object Map Iterator]", Object.prototype.toString.call((new Map())[Symbol.iterator]()), "toString should have tag Map Iterator");
            assert.areEqual("[object Set Iterator]", Object.prototype.toString.call((new Set())[Symbol.iterator]()), "toString should have tag Set Iterator");
       }
    },
    /* Subclassing disabled in Windows10 enable once we have full support for classes.
    {
       name: "sub class Array.toString",
       body: function () {
            class MyArray extends Array {
                constructor() {
                   super();
                }
            }
            MyArray.prototype[Symbol.toStringTag] = "MyArray";

            var m = new MyArray();
            assert.areEqual("[object Array]", Object.prototype.toString.call(new Array()), "toString should have tag Array");
            assert.areEqual("[object ~MyArray]", Object.prototype.toString.call(m), "toString should have tag ~MyArray");
       }
    },
    {
       name: "sub class Boolean.toString",
       body: function () {
            class MyBoolean extends Boolean {
                constructor() {
                   super();
                }
            }
            MyBoolean.prototype[Symbol.toStringTag] = "MyBoolean";

            var m = new MyBoolean();
            assert.areEqual("[object Boolean]", Object.prototype.toString.call(new Boolean()), "toString should have tag Boolean");
            assert.areEqual("[object ~MyBoolean]", Object.prototype.toString.call(m), "toString should have tag ~MyBoolean");
       }
    },
    {
       name: "sub class Date.toString",
       body: function () {
            class MyDate extends Date {
                constructor() {
                   super();
                }
            }
            MyDate.prototype[Symbol.toStringTag] = "MyDate";

            var m = new MyDate();
            assert.areEqual("[object Date]", Object.prototype.toString.call(new Date()), "toString should have tag Date");
            assert.areEqual("[object ~MyDate]", Object.prototype.toString.call(m), "toString should have tag ~MyDate");
       }
    },
    {
       name: "sub class Error.toString",
       body: function () {
            class MyError extends Error {
                constructor() {
                   super();
                }
            }
            MyError.prototype[Symbol.toStringTag] = "MyError";

            var m = new MyError();
            assert.areEqual("[object Error]", Object.prototype.toString.call(new Error()), "toString should have tag Error");
            assert.areEqual("[object ~MyError]", Object.prototype.toString.call(m), "toString should have tag ~MyError");
       }
    },
    {
       name: "sub class Function.toString",
       body: function () {
            class MyFunction extends Function {
                constructor() {
                   super();
                }
            }
            MyFunction[Symbol.toStringTag] = "MyFunction";

            assert.areEqual("[object Function]", Object.prototype.toString.call(Function), "toString should have tag Function");
            assert.areEqual("[object ~MyFunction]", Object.prototype.toString.call(MyFunction), "toString should have tag ~MyFunction");
       }
    },
    {
       name: "sub class Number.toString",
       body: function () {
            class MyNumber extends Number {
                constructor() {
                   super();
                }
            }
            MyNumber.prototype[Symbol.toStringTag] = "MyNumber";

            var m = new MyNumber();
            assert.areEqual("[object Number]", Object.prototype.toString.call(new Number()), "toString should have tag Number");
            assert.areEqual("[object ~MyNumber]", Object.prototype.toString.call(m), "toString should have tag ~MyNumber");
       }
    },
    {
       name: "sub class RegExp.toString",
       body: function () {
            class MyRegExp extends RegExp {
                constructor() {
                   super();
                }
            }
            MyRegExp.prototype[Symbol.toStringTag] = "MyRegExp";

            var m = new MyRegExp();
            assert.areEqual("[object RegExp]", Object.prototype.toString.call(new RegExp()), "toString should have tag RegExp");
            assert.areEqual("[object ~MyRegExp]", Object.prototype.toString.call(m), "toString should have tag ~MyRegExp");
       }
    },
    {
       name: "sub class String.toString",
       body: function () {
            class MyString extends String {
                constructor() {
                   super();
                }
            }

            MyString.prototype[Symbol.toStringTag] = "MyString";

            var m = new MyString();
            assert.areEqual("[object String]", Object.prototype.toString.call(new String()), "toString should have tag String");
            assert.areEqual("[object ~MyString]", Object.prototype.toString.call(m, "toString should have tag ~MyString"));
       }
    },
    {
       name: "sub class String.toString with bad tag",
       body: function () {
            class MyString extends String {
                constructor() {
                   super();
                }
            }
            MyString.prototype[Symbol.toStringTag] = {};

            var m = new MyString();
            assert.areEqual("[object String]", Object.prototype.toString.call(new String()), "toString should have tag String");
            assert.areEqual("[object ???]", Object.prototype.toString.call(m), "toString should have tag ???");
       }
    },
        {
       name: "classes with no tag & tag on instance tests",
       body: function () {
            class MyString extends String {
                constructor() {
                   super();
                }
            }

            var m = new MyString();
            assert.areEqual("[object String]", Object.prototype.toString.call(new String()), "toString should have tag String");
            assert.areEqual("[object String]", Object.prototype.toString.call(m), "MyString's toString should have tag String");

            class MyClass {
                constructor() { }
            }

            var m = new MyClass();
            var m2 = new MyClass();
            assert.areEqual("[object Object]", Object.prototype.toString.call(m), "toString should have tag Object");

            m2[Symbol.toStringTag] = "MyClassInstance";
            assert.areEqual("[object MyClassInstance]", Object.prototype.toString.call(m2), "toString should have tag MyClassInstance");
            assert.areEqual("[object Object]", Object.prototype.toString.call(m), "toString should have tag Object m2 only changed instance tag");

            MyClass.prototype[Symbol.toStringTag] = "MyClass";
            assert.areEqual("[object MyClassInstance]", Object.prototype.toString.call(m2), "toString should have tag MyClassInstance b\c MyClass is higher in the prototype chain");
            assert.areEqual("[object MyClass]", Object.prototype.toString.call(m), "toString should have tag MyClass b\c prototype tag changed");
       }
    },
    */
    {
       name: "throws test case",
       body: function () {
            var a = {};
            a[Symbol.toStringTag] = "aObject";
            assert.areEqual("aObject", a[Symbol.toStringTag], "check Tag is aObject");

            assert.areEqual("[object aObject]", Object.prototype.toString.call(a), "check toString has tag aObject");
            Object.defineProperty(a, Symbol.toStringTag, {get : function() { throw 10;}});

            try {
                a[Symbol.toStringTag];
            } catch(e) {
                assert.areEqual("[object ???]", Object.prototype.toString.call(a), "JS exceptions should have tag ???");
            }
            assert.areEqual("[object ???]", Object.prototype.toString.call(a), "JS exceptions should have tag ??? regardless of try catch");
       }
    },
    {
        name: "VSO OS Bug 1160433: embedded null characters in toStringTag property value",
        body: function () {
            var o = {
                [Symbol.toStringTag]: "before\0after"
            };

            assert.areEqual("[object before\0after]", o.toString(), "ToString implementation handles embedded null characters in @@toStringTag property value");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
