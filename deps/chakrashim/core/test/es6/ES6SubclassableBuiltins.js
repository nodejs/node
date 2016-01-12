//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Subclassable tests -- verifies subclass construction behavior

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Subclass of Boolean",
        body: function () {
            class MyBoolean extends Boolean {
                constructor(...val) {
                    super(...val);
                    this.prop = 'mybool';
                }

                method() {
                    return this.prop;
                }
            }

            assert.areEqual('mybool', new MyBoolean(true).method(), "Subclass of Boolean has correct methods and properties");
            assert.isTrue(new MyBoolean(true) == true, "Subclass of Boolean object has correct boolean value");
            assert.isTrue(new MyBoolean(false) == false, "Subclass of Boolean object has correct boolean value");
        }
    },
    {
        name: "Subclass of Error",
        body: function () {
            function verifySubclassError(constructor, constructorName) {
                class MyError extends constructor {
                    constructor(...val) {
                        super(...val);
                        this.prop = 'myerrorsubclass of ' + constructorName;
                    }

                    method() {
                        return this.prop;
                    }
                }

                assert.areEqual('myerrorsubclass of ' + constructorName, new MyError('message').method(), "Subclass of " + constructorName + " has correct methods and properties");
                assert.areEqual(constructorName + ": message", new MyError('message').toString(), "Subclass of " + constructorName + " has correct message value");
            }

            verifySubclassError(Error, 'Error');
            verifySubclassError(EvalError, 'EvalError');
            verifySubclassError(RangeError, 'RangeError');
            verifySubclassError(ReferenceError, 'ReferenceError');
            verifySubclassError(SyntaxError, 'SyntaxError');
            verifySubclassError(TypeError, 'TypeError');
            verifySubclassError(URIError, 'URIError');
        }
    },
    {
        name: "Subclass of Number",
        body: function () {
            class MyNumber extends Number {
                constructor(...val) {
                    super(...val);
                    this.prop = 'mynumber';
                }

                method() {
                    return this.prop;
                }
            }

            assert.areEqual('mynumber', new MyNumber(0).method(), "Subclass of Number has correct methods and properties");
            assert.isTrue(new MyNumber(123) == 123, "Subclass of Number object has correct value");
            assert.isTrue(new MyNumber() == 0, "MyNumber constructor calls super with no argument should behave the same way as Number constructor and return NaN!");
        }
    },
    {
        name: "Subclass of Array",
        body: function () {
            class MyArray extends Array {
                constructor(...val) {
                    super(...val);
                    this.prop = 'myarray';
                }

                method() {
                    return this.prop;
                }
            }

            assert.areEqual('myarray', new MyArray().method(), "Subclass of Array has correct methods and properties");
            assert.areEqual(0, new MyArray().length, "Subclass of Array object has correct length when constructor called with no arguments");
            assert.areEqual(100, new MyArray(100).length, "Subclass of Array object has correct length when constructor called with single numeric argument");
            assert.areEqual(50, new MyArray(50.0).length, "Subclass of Array object has correct length when constructor called with single float argument");
            assert.areEqual(1, new MyArray('something').length, "Subclass of Array object has correct length when constructor called with single non-numeric argument");
            assert.areEqual('something', new MyArray('something')[0], "Subclass of Array object has correct length when constructor called with single non-numeric argument");

            var a = new MyArray(1,2,3);
            assert.areEqual(3, a.length, "Subclass of Array object has correct length when constructor called with multiple arguments");
            assert.areEqual(1, a[0], "Subclass of Array object has correct values when constructor called with multiple arguments");
            assert.areEqual(2, a[1], "Subclass of Array object has correct values when constructor called with multiple arguments");
            assert.areEqual(3, a[2], "Subclass of Array object has correct values when constructor called with multiple arguments");

            assert.isTrue(Array.isArray(a), "Subclass of Array is an array as tested via Array.isArray");
        }
    },
    {
        name: "Subclass of Array - proto chain",
        body: function () {
            class MyArray extends Array
            {
                constructor(...args) { super(...args); }
                getFirstElement() { return this.length > 0 ? this[0] : undefined; }
                getLastElement() { return this.length > 0 ? this[this.length-1] : undefined; }
            }
            class OurArray extends MyArray
            {
                constructor(...args) { super(...args); }
                getLength() { return this.length; }
            }

            function verifyProtoChain(obj, length, newElement, firstElement)
            {
                assert.areEqual(false, obj instanceof Function, "Subclass of Array is not a function object");
                assert.areEqual(true, obj instanceof Array, "Subclass of Array is an Array");
                assert.areEqual(true, obj instanceof MyArray, "Subclass of Array is a 'MyArray' instance");
                assert.areEqual(true, obj instanceof OurArray, "Subclass of Array is a 'OurArray' instance");

                assert.areEqual(OurArray.prototype, obj.__proto__, "obj's [[Prototype]] slot points to OurArray.prototype");
                assert.areEqual(MyArray.prototype, obj.__proto__.__proto__, "obj's 2nd-order [[Prototype]] points to MyArray.prototype");
                assert.areEqual(Array.prototype, obj.__proto__.__proto__.__proto__, "obj's 3rd-order [[Prototype]] chain points to Array.prototype");

                assert.areEqual(length, obj.length, "Subclass of Array is a 'OurArray' instance");
                obj[length] = newElement;
                assert.areEqual(length + 1, obj.length, "Subclass of Array is a 'OurArray' instance");

                assert.areEqual(length + 1, obj.getLength(), "obj.getLength() returns "+ (length + 1));
                assert.areEqual(firstElement, obj.getFirstElement(), "obj.getFirstElement() returns "+ firstElement);
                assert.areEqual(newElement, obj.getLastElement(), "obj.getLastElement() returns "+ newElement);
            }

            assert.areEqual(MyArray, OurArray.__proto__, "OurArray's [[Prototype]] slot points to MyArray");
            assert.areEqual(Array, MyArray.__proto__, "MyArray's [[Prototype]] slot points to Array");

            verifyProtoChain(new OurArray(), 0, 1, 1);
            verifyProtoChain(new OurArray('e'), 1, 'element', 'e');
            verifyProtoChain(new OurArray('xyz',2), 2, function(){}, 'xyz');
            verifyProtoChain(new OurArray(1,2,3), 3, 4, 1);
            verifyProtoChain(new OurArray('a','b','c','d'), 4, 'e', 'a');
            verifyProtoChain(new OurArray(100), 100, 'element', undefined);
        }
    },
    {
        name: "Subclass of built-in constructors - verify proto chain",
        body: function () {
            function testProtoChain (Type, isFunction, ctorArgs)
            {
                class MyType extends Type
                {
                    constructor(...args) { super(...args); this.prop1="method1"; }
                    method1() { return ">"+this.prop1; }
                }
                class OurType extends MyType
                {
                    constructor(...args) { super(...args); this.prop0="method0"; }
                    method0() { return ">"+this.prop0; }
                }

                function verifyProtoChain(obj)
                {
                    assert.areEqual(isFunction, obj instanceof Function, "Subclass of "+ Type.name +" is" + (isFunction ? "" : " not")  + " a function object");
                    assert.areEqual(true, obj instanceof Type, "Subclass of " + Type.name + " is an instance of " + Type.name);
                    assert.areEqual(true, obj instanceof MyType, "Subclass of " + Type.name + " is an instance of 'MyType'");
                    assert.areEqual(true, obj instanceof OurType, "Subclass of " + Type.name + " is an instance of 'OurType'");

                    assert.areEqual(OurType.prototype, obj.__proto__, "obj's [[Prototype]] slot points to OurType.prototype");
                    assert.areEqual(MyType.prototype, obj.__proto__.__proto__, "obj's 2nd-order [[Prototype]] points to MyType.prototype");
                    assert.areEqual(Type.prototype, obj.__proto__.__proto__.__proto__, "obj's 3rd-order [[Prototype]] chain points to Type.prototype");

                    assert.areEqual(">method0", obj.method0(), "obj");
                    assert.areEqual(">method1", obj.method1(), "obj");
                }

                assert.areEqual(MyType, OurType.__proto__, "OurType's [[Prototype]] slot points to MyType");
                assert.areEqual(Type, MyType.__proto__, "MyType's [[Prototype]] slot points to Type");

                verifyProtoChain(eval("new OurType("+ctorArgs+")"));
            }

            function testReflectConstructNewTarget (Type, isFunction, ctorArgs)
            {
                class MyType extends Type {}
                let obj = Reflect.construct(Type, eval("["+ctorArgs+"]"), MyType);

                assert.areEqual(true, obj instanceof MyType, "new.target should be available in built-in subclassable constructor " + Type.name);
            }

            function forEachBuiltinSubclassable(test)
            {
                let GeneratorFunction = (function* g() {}).constructor;
                let TypedArray = Int8Array.__proto__;

                test(Array, false, "");
                test(ArrayBuffer, false, "");
                test(Boolean, false, "");
                test(DataView, false, "new ArrayBuffer()");
                test(Date, false, "");
                test(Error, false, "");
                test(  EvalError, false, "");
                test(  RangeError, false, "");
                test(  ReferenceError, false, "");
                test(  SyntaxError, false, "");
                test(  TypeError, false, "");
                test(  URIError, false, "");
                test(Function, true, "");
                test(GeneratorFunction, true, "");
                test(Map, false, "");
                test(Number, false, "");
                test(Object, false, "");
                test(Promise, false, "function() {}");
                test(RegExp, false, "");
                test(Set, false, "");
                test(String, false, "");
                assert.throws( function() { test(Symbol, false, ""); }, TypeError,
                    "Subclasses of Symbol cannot be instantiated", "Function is not a constructor");
                assert.throws( function() { test(TypedArray, false, ""); }, TypeError,
                    "Subclasses of typed array constructor cannot be instantiated", "Typed array constructor argument is invalid");
                test(  Int8Array, false, "");
                test(  Int16Array, false, "");
                test(  Int32Array, false, "");
                test(  Uint8Array, false, "");
                test(  Uint8ClampedArray, false, "");
                test(  Uint16Array, false, "");
                test(  Uint32Array, false, "");
                test(  Float32Array, false, "");
                test(  Float64Array, false, "");
                test(WeakMap, false, "");
                test(WeakSet, false, "");
            }
            forEachBuiltinSubclassable(testProtoChain);
            forEachBuiltinSubclassable(testReflectConstructNewTarget);
       }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
