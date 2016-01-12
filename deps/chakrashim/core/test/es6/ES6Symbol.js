//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Symbol tests -- verifies the API shape and basic functionality

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

function VerfiyToPropertyKey(key) {
    var obj = {};

    assert.isFalse(obj.hasOwnProperty(key), "Object#hasOwnProperty uses ToPropertyKey. Initially we don't have the property.");
    assert.doesNotThrow(function() { Object.defineProperty(obj, key, { value: 'something', enumerable: true }); }, "Object.defineProperty uses ToPropertyKey. Property is added to the object");

    assert.isTrue(obj.hasOwnProperty(key), "Object#hasOwnProperty uses ToPropertyKey on argument. We should have the property now.");
    assert.isTrue(obj.propertyIsEnumerable(key), "Object#propertyIsEnumerable uses ToPropertyKey.");
    assert.areNotEqual(undefined, Object.getOwnPropertyDescriptor(obj, key), "Object.getOwnPropertyDescriptor uses ToPropertyKey");

    obj = {};
    obj.__defineGetter__(key, () => { return 2;} );
    assert.isTrue(obj.hasOwnProperty(key), "Object#__defineGetter__ uses ToPropertyKey. Property is added to the object");

    obj = {};
    obj.__defineSetter__(key, () => { return 2;} );
    assert.isTrue(obj.hasOwnProperty(key), "Object#__defineSetter__ uses ToPropertyKey. Property is added to the object");

    var count = 0;
    obj = Object.defineProperty({}, key, { set(v) { assert.areEqual('abc', v, "Setter called with correct arg"); count++; } });
    var set = obj.__lookupSetter__(key);
    assert.areEqual('function', typeof set, "Object#__lookupSetter__ uses ToPropertyKey. Make sure we added a setter.");
    set('abc');
    assert.areEqual(1, count, "Correct setter was called.");

    obj = Object.defineProperty({}, key, { get() { return 'abc'; } });
    var get = obj.__lookupGetter__(key);
    assert.areEqual('function', typeof get, "Object#__lookupGetter__ uses ToPropertyKey. Make sure we added a getter.");
    assert.areEqual('abc', get(), "Correct getter was called.");

    obj = {};
    assert.doesNotThrow(function() { Reflect.set(obj, key, 'abc'); }, "Reflect.set uses ToPropertyKey on argument. We should have set property");
    assert.areEqual('abc', Reflect.get(obj, key), "Reflect.get also uses ToPropertyKey. We should return the same property");
    assert.isTrue(Reflect.deleteProperty(obj, key), "Reflect.deleteProperty uses ToPropertyKey. We should successfully remove the property here");
    assert.isFalse(Reflect.has(obj, key), "Reflect.has uses ToPropertyKey. We deleted this property, so we should not have it now.");
    assert.doesNotThrow(function() { Reflect.defineProperty(obj, key, { value: 'def', enumerable: true }); }, "Reflect.defineProperty uses ToPropertyKey. It should have created a new property");
    assert.areEqual('def', Reflect.get(obj, key), "Reflect.get also uses ToPropertyKey. We should return the same property");
    assert.areNotEqual(undefined, Reflect.getOwnPropertyDescriptor(obj, key), "Reflect.getOwnPropertyDescriptor uses ToPropertyKey. It should return a real descriptor object");

    obj = {};
    assert.doesNotThrow(function() { obj[key] = 123; }, "Ordinary [[Set]] eventually uses ToPropertyKey. Property is added to the object");
    assert.areEqual(123, obj[key], "Ordinary [[Get]] also eventually goes down to ToPropertyKey which should return us the same value");
    assert.isTrue(obj.hasOwnProperty(key), "Verify we added the property under key and not some stringified version of key");
}

var tests = [
    {
        name: "Symbol is a constructor object and has correct shape",
        body: function () {
            assert.isTrue(Symbol !== undefined, "Symbol is defined");
            assert.areEqual('function', typeof Symbol, "typeof Symbol === 'function'");
            assert.areEqual(0, Symbol.length, "Symbol.length === 0");

            assert.areEqual('function', typeof Symbol.toString, "typeof Symbol.toString === 'function'");
            assert.areEqual('function', typeof Symbol.valueOf, "typeof Symbol.valueOf === 'function'");

            assert.areEqual('function', typeof Symbol.for, "typeof Symbol.for === 'function'");
            assert.areEqual(1, Symbol.for.length, "Symbol.for.length === 1");
            descriptor = Object.getOwnPropertyDescriptor(Symbol, 'for');
            assert.isTrue(descriptor.writable, 'Symbol.for.descriptor.writable == true');
            assert.isFalse(descriptor.enumerable, 'Symbol.for.descriptor.enumerable == false');
            assert.isTrue(descriptor.configurable, 'Symbol.for.descriptor.configurable == true');

            assert.areEqual('function', typeof Symbol.keyFor, "typeof Symbol.keyFor === 'function'");
            assert.areEqual(1, Symbol.keyFor.length, "Symbol.keyFor.length === 1");
            descriptor = Object.getOwnPropertyDescriptor(Symbol, 'keyFor');
            assert.isTrue(descriptor.writable, 'Symbol.keyFor.descriptor.writable == true');
            assert.isFalse(descriptor.enumerable, 'Symbol.keyFor.descriptor.enumerable == false');
            assert.isTrue(descriptor.configurable, 'Symbol.keyFor.descriptor.configurable == true');
        }
    },
    {
        name: "Symbol prototype has expected shape",
        body: function() {
            assert.isTrue(Symbol === Symbol.prototype.constructor, "Symbol === Symbol.prototype.constructor");
            descriptor = Object.getOwnPropertyDescriptor(Symbol, 'prototype');

            assert.isFalse(descriptor.writable, 'Symbol.prototype.descriptor.writable == false');
            assert.isFalse(descriptor.enumerable, 'Symbol.prototype.descriptor.enumerable == false');
            assert.isFalse(descriptor.configurable, 'Symbol.prototype.descriptor.configurable == false');

            assert.areEqual('function', typeof Symbol.prototype.toString, "typeof Symbol.prototype.toString === 'function'");
            descriptor = Object.getOwnPropertyDescriptor(Symbol.prototype, 'toString');

            assert.isTrue(descriptor.writable, 'Symbol.prototype.toString.descriptor.writable == true');
            assert.isFalse(descriptor.enumerable, 'Symbol.prototype.toString.descriptor.enumerable == false');
            assert.isTrue(descriptor.configurable, 'Symbol.prototype.toString.descriptor.configurable == true');

            assert.areEqual('function', typeof Symbol.prototype.valueOf, "typeof Symbol.prototype.valueOf === 'function'");
            descriptor = Object.getOwnPropertyDescriptor(Symbol.prototype, 'valueOf');

            assert.isTrue(descriptor.writable, 'Symbol.prototype.valueOf.descriptor.writable == true');
            assert.isFalse(descriptor.enumerable, 'Symbol.prototype.valueOf.descriptor.enumerable == false');
            assert.isTrue(descriptor.configurable, 'Symbol.prototype.valueOf.descriptor.configurable == true');

            assert.areEqual('function', typeof Symbol.prototype[Symbol.toPrimitive], "typeof Symbol.prototype[@@toPrimitive] === 'function'");
            assert.areEqual(1, Symbol.prototype[Symbol.toPrimitive].length, "Symbol.prototype[@@toPrimitive].length === 1");
            descriptor = Object.getOwnPropertyDescriptor(Symbol.prototype, Symbol.toPrimitive);

            assert.isFalse(descriptor.writable, 'Symbol.prototype[@@toPrimitive].descriptor.writable == false');
            assert.isFalse(descriptor.enumerable, 'Symbol.prototype[@@toPrimitive].descriptor.enumerable == false');
            assert.isTrue(descriptor.configurable, 'Symbol.prototype[@@toPrimitive].descriptor.configurable == true');

            var functionToString = Symbol.prototype[Symbol.toPrimitive].toString();
            var actualName = functionToString.substring(9, functionToString.indexOf('('));
            assert.areEqual('[Symbol.toPrimitive]', actualName, "Symbol[@@toPrimitive].name == '[Symbol.toPrimitive]'");

            assert.areEqual('string', typeof Symbol.prototype[Symbol.toStringTag], "typeof Symbol.prototype[@@toStringTag] === 'string'");
            descriptor = Object.getOwnPropertyDescriptor(Symbol.prototype, Symbol.toStringTag);

            assert.isFalse(descriptor.writable, 'Symbol.prototype[@@toStringTag].descriptor.writable == false');
            assert.isFalse(descriptor.enumerable, 'Symbol.prototype[@@toStringTag].descriptor.enumerable == false');
            assert.isTrue(descriptor.configurable, 'Symbol.prototype[@@toStringTag].descriptor.configurable == true');
            assert.areEqual('Symbol', Symbol.prototype[Symbol.toStringTag], "Symbol.prototype[@@toStringTag] === 'Symbol'");
        }
    },
    {
        name: "Symbol constructor and prototype built-ins",
        body: function() {
            var x = Symbol("x");
            var y = Symbol("y");

            // toPrimitive() behavior
            assert.areEqual(x, x[Symbol.toPrimitive](), "x == x[Symbol.toPrimitive]()");
            assert.areEqual(x, x[Symbol.toPrimitive].call(x), "x == x[Symbol.toPrimitive].call(x)");
            assert.areEqual(y, x[Symbol.toPrimitive].call(y), "y == x[Symbol.toPrimitive].call(y)");
            assert.isFalse(x == x[Symbol.toPrimitive].call(y), "x != x[Symbol.toPrimitive].call(y)");
            assert.areEqual(x, Symbol.prototype[Symbol.toPrimitive].call(x), "x == Symbol.prototype[Symbol.toPrimitive].call(x)");

            // TypeError scenarios
            assert.throws(function () { x[Symbol.toPrimitive].call("x") }, TypeError, "x[Symbol.toPrimitive].call('x'), toPrimitive throws TypeError for values that does not have SymbolData", "Symbol[Symbol.toPrimitive]: invalid argument");
            assert.throws(function () { Symbol.prototype[Symbol.toPrimitive]() }, TypeError, "toPrimitive throws TypeError if no arguments are passed", "Symbol[Symbol.toPrimitive]: invalid argument");
            assert.throws(function () { Symbol.prototype[Symbol.toPrimitive].call("") }, TypeError, "toPrimitive throws TypeError for values that does not have SymbolData", "Symbol[Symbol.toPrimitive]: invalid argument");
            assert.throws(function () { Symbol.prototype[Symbol.toPrimitive].call(null) }, TypeError, "toPrimitive throws TypeError for null", "Symbol[Symbol.toPrimitive]: invalid argument");
            assert.throws(function () { Symbol.prototype[Symbol.toPrimitive].call(undefined) }, TypeError, "toPrimitive throws TypeError for undefined", "Symbol[Symbol.toPrimitive]: invalid argument");
            assert.throws(function () { Symbol.prototype[Symbol.toPrimitive].call({}) }, TypeError, "toPrimitive throws TypeError for object", "Symbol[Symbol.toPrimitive]: invalid argument");

            var z = Object(y);
            assert.areEqual(y, Symbol.prototype[Symbol.toPrimitive].call(z), "y == Symbol.prototype[Symbol.toPrimitive].call(z)");
            assert.isFalse(Object(x) == Symbol.prototype[Symbol.toPrimitive].call(z), "Object(x) != Symbol.prototype[Symbol.toPrimitive].call(z)");
        }
    },
    {
        name: "Symbol constructor has the well-known symbols as properties",
        body: function() {
            var descriptor;

            assert.isTrue(Symbol.hasInstance !== undefined, "Symbol.hasInstance !== undefined");
            assert.areEqual('symbol', typeof Symbol.hasInstance, "typeof Symbol.hasInstance === 'symbol'");
            descriptor = Object.getOwnPropertyDescriptor(Symbol, 'hasInstance');

            assert.isFalse(descriptor.writable, 'Symbol.hasInstance.descriptor.writable == false');
            assert.isFalse(descriptor.enumerable, 'Symbol.hasInstance.descriptor.enumerable == false');
            assert.isFalse(descriptor.configurable, 'Symbol.hasInstance.descriptor.configurable == false');

            assert.isTrue(Symbol.isConcatSpreadable !== undefined, "Symbol.isConcatSpreadable !== undefined");
            assert.areEqual('symbol', typeof Symbol.isConcatSpreadable, "typeof Symbol.isConcatSpreadable === 'symbol'");
            descriptor = Object.getOwnPropertyDescriptor(Symbol, 'isConcatSpreadable');

            assert.isFalse(descriptor.writable, 'Symbol.isConcatSpreadable.descriptor.writable == false');
            assert.isFalse(descriptor.enumerable, 'Symbol.isConcatSpreadable.descriptor.enumerable == false');
            assert.isFalse(descriptor.configurable, 'Symbol.isConcatSpreadable.descriptor.configurable == false');

            assert.isTrue(Symbol.iterator !== undefined, "Symbol.iterator !== undefined");
            assert.areEqual('symbol', typeof Symbol.iterator, "typeof Symbol.iterator === 'symbol'");
            descriptor = Object.getOwnPropertyDescriptor(Symbol, 'iterator');

            assert.isFalse(descriptor.writable, 'Symbol.iterator.descriptor.writable == false');
            assert.isFalse(descriptor.enumerable, 'Symbol.iterator.descriptor.enumerable == false');
            assert.isFalse(descriptor.configurable, 'Symbol.iterator.descriptor.configurable == false');

            assert.isTrue(Symbol.toPrimitive !== undefined, "Symbol.toPrimitive !== undefined");
            assert.areEqual('symbol', typeof Symbol.toPrimitive, "typeof Symbol.toPrimitive === 'symbol'");
            descriptor = Object.getOwnPropertyDescriptor(Symbol, 'toPrimitive');

            assert.isFalse(descriptor.writable, 'Symbol.toPrimitive.descriptor.writable == false');
            assert.isFalse(descriptor.enumerable, 'Symbol.toPrimitive.descriptor.enumerable == false');
            assert.isFalse(descriptor.configurable, 'Symbol.toPrimitive.descriptor.configurable == false');

            assert.isTrue(Symbol.toStringTag !== undefined, "Symbol.toStringTag !== undefined");
            assert.areEqual('symbol', typeof Symbol.toStringTag, "typeof Symbol.toStringTag === 'symbol'");
            descriptor = Object.getOwnPropertyDescriptor(Symbol, 'toStringTag');

            assert.isFalse(descriptor.writable, 'Symbol.toStringTag.descriptor.writable == false');
            assert.isFalse(descriptor.enumerable, 'Symbol.toStringTag.descriptor.enumerable == false');
            assert.isFalse(descriptor.configurable, 'Symbol.toStringTag.descriptor.configurable == false');

            assert.isTrue(Symbol.unscopables !== undefined, "Symbol.unscopables !== undefined");
            assert.areEqual('symbol', typeof Symbol.unscopables, "typeof Symbol.unscopables === 'symbol'");
            descriptor = Object.getOwnPropertyDescriptor(Symbol, 'unscopables');

            assert.isFalse(descriptor.writable, 'Symbol.unscopables.descriptor.writable == false');
            assert.isFalse(descriptor.enumerable, 'Symbol.unscopables.descriptor.enumerable == false');
            assert.isFalse(descriptor.configurable, 'Symbol.unscopables.descriptor.configurable == false');

            assert.isTrue(Symbol.species !== undefined, "Symbol.species !== undefined");
            assert.areEqual('symbol', typeof Symbol.species, "typeof Symbol.species === 'symbol'");
            descriptor = Object.getOwnPropertyDescriptor(Symbol, 'species');

            assert.isFalse(descriptor.writable, 'Symbol.species.descriptor.writable == false');
            assert.isFalse(descriptor.enumerable, 'Symbol.species.descriptor.enumerable == false');
            assert.isFalse(descriptor.configurable, 'Symbol.species.descriptor.configurable == false');
        }
    },
    {
        name: "Symbol primitive toString should throw a type error",
        body: function() {
            assert.throws(function() { 'string' + Symbol.iterator; }, TypeError, "Symbol primitives throw on implicit string conversion", "Object doesn't support property or method 'ToString'");
        }
    },
    {
        name: "String(symbol) behavior",
        body: function() {
            assert.areEqual('Symbol(description)', String(Symbol('description')), "String(Symbol('description')) === 'Symbol(description)'");
            assert.throws(function () { new String(Symbol('description')); }, TypeError, "Symbol as an argument to new String() throws", "Object doesn't support property or method 'ToString'");
        }
    },
    {
        name: "Symbol object toString produces a human-readable name",
        body: function() {
            assert.areEqual('Symbol(Symbol.hasInstance)', Object(Symbol.hasInstance).toString(), "Object(Symbol.hasInstance).toString() === 'Symbol(Symbol.hasInstance)'");
            assert.areEqual('Symbol(Symbol.isConcatSpreadable)', Object(Symbol.isConcatSpreadable).toString(), "Object(Symbol.isConcatSpreadable).toString() === 'Symbol(Symbol.isConcatSpreadable)'");
            assert.areEqual('Symbol(Symbol.iterator)', Object(Symbol.iterator).toString(), "Object(Symbol.iterator).toString() === 'Symbol(Symbol.iterator)'");
            assert.areEqual('Symbol(Symbol.toPrimitive)', Object(Symbol.toPrimitive).toString(), "Object(Symbol.toPrimitive).toString() === 'Symbol(Symbol.toPrimitive)'");
            assert.areEqual('Symbol(Symbol.toStringTag)', Object(Symbol.toStringTag).toString(), "Object(Symbol.toStringTag).toString() === 'Symbol(Symbol.toStringTag)'");
            assert.areEqual('Symbol(Symbol.unscopables)', Object(Symbol.unscopables).toString(), "Object(Symbol.unscopables).toString() === 'Symbol(Symbol.unscopables)'");

            assert.areEqual('Symbol()', Object(Symbol()).toString(), "Object(Symbol()).toString() === 'Symbol()'");
            assert.areEqual("Symbol(Some kind of long string description\n\n)", Object(Symbol("Some kind of long string description\n\n")).toString(), "Object(Symbol(\"Some kind of long string description\n\n\")).toString() === 'Symbol(Some kind of long string description\n\n)'");
        }
    },
    {
        name: "typeof a symbol primitive is 'symbol'",
        body: function() {
            assert.areEqual('symbol', typeof Symbol('mysymbol'), "typeof Symbol('mysymbol') === 'symbol'");
            assert.areEqual('symbol', typeof Symbol(''), "typeof Symbol('') === 'symbol'");
            assert.areEqual('symbol', typeof Symbol(), "typeof Symbol() === 'symbol'");
        }
    },
    {
        name: "new Symbol throws",
        body: function() {
            assert.throws(function () { new Symbol() }, TypeError, "new Symbol throws TypeError when it has no parameter", "Function is not a constructor");
            assert.throws(function () { new Symbol('anything') }, TypeError, "new Symbol throws TypeError when it has a string parameter", "Function is not a constructor");
        }
    },
    {
        name: "Symbols with single-character descriptions (these are special-cased in ThreadContext)",
        body: function() {
            assert.isTrue(Symbol('s') !== Symbol('s'), "We are able to create multiple symbols with the same single-character description and they are not equal");
        }
    },
    {
        name: "Symbol strict equality with other symbols",
        body: function() {
            assert.isTrue(Symbol('something') !== Symbol('something'), "Symbol('something') !== Symbol('something')");
            assert.isTrue(Symbol('') !== Symbol(''), "Symbol('') !== Symbol('')");
            assert.isTrue(Symbol() !== Symbol(), "Symbol() !== Symbol()");

            var my1 = Symbol('my');
            assert.isTrue(my1 === my1, "Generated symbol should equal itself");
            var my2 = my1;
            assert.isTrue(my1 === my2, "Assignment to another Var should still equal the original symbol");

            var o1 = Object(my1);
            var o2 = Object(my1);
            assert.isTrue(o1 !== o2, "Box objects should not be equal for the same symbol");
            assert.isTrue(o1 !== my1, "Box object should not be equal to the symbol primitive");
            assert.isTrue(o1.valueOf() === o2.valueOf(), "Unboxing objects wrapping the same symbol primitive should result in the same symbol returned from valueOf()");

            var o3 = Object(Symbol('another'));
            assert.isTrue(o1 !== o3, "Box objects should not be equal for different symbol primitives");

            var my3 = o1.valueOf();
            assert.isTrue(my1 === my3, "Unboxed symbol should be equal to original primitive");

            assert.isTrue(Symbol.iterator !== Symbol('iterator'), "Symbol.iterator !== Symbol('iterator')");

            assert.isTrue(Object(Symbol('sym')).valueOf() !== Object(Symbol('sym')).valueOf(), "Different symbol primitives boxed and unboxed should not be equal to each other");
        }
    },
    {
        name: "Symbol strict equality with other types",
        body: function() {
            var sym = Symbol('my');

            assert.isFalse(sym === 'string', "sym !== 'string'");
            assert.isFalse(sym === undefined, "sym !== undefined");
            assert.isFalse(sym === null, "sym !== null");
            assert.isFalse(sym === true, "sym !== true");
            assert.isFalse(sym === false, "sym !== true");
            assert.isFalse(sym === [], "sym !== []");
            assert.isFalse(sym === {}, "sym !== {}");
            assert.isTrue(sym === sym, "sym === sym");

            assert.isFalse('string' === sym, "'string' !== sym");
            assert.isFalse(undefined === sym, "undefined !== sym");
            assert.isFalse(null === sym, "null !== sym");
            assert.isFalse(true === sym, "true !== sym");
            assert.isFalse(false === sym, "false !== sym");
            assert.isFalse([] === sym, "[] !== sym");
            assert.isFalse({} === sym, "{} !== sym");
        }
    },
    {
        name: "Symbol equality with other types",
        body: function() {
            var sym = Symbol('my');

            assert.isFalse(sym == 'string', "ToString(symbol) throws so this should be false");
            assert.isFalse(sym == undefined, "sym != undefined");
            assert.isFalse(sym == null, "sym != null");
            assert.isFalse(sym == true, "symbol != true");
            assert.isFalse(sym == false, "symbol != false");
            assert.isFalse(sym == [], "sym != []");
            assert.isFalse(sym == {}, "sym != {}");
            assert.isTrue(sym == sym, "sym == sym");

            assert.isFalse('string' == sym, "ToString(symbol) throws so this should be false");
            assert.isFalse(undefined == sym, "undefined != sym");
            assert.isFalse(null == sym, "null != sym");
            assert.isFalse(true == sym, "true != sym");
            assert.isFalse(false == sym, "false != sym");
            assert.isFalse([] == sym, "[] != sym");
            assert.isFalse({} == sym, "{} != sym");
        }
    },
    {
        name: "Symbol equality with auto-boxed Symbols",
        body: function() {
            var sym = Symbol('my');

            assert.isTrue(sym == Object(sym), "Auto-boxed symbol is equal to that symbol");
            assert.isTrue(Object(sym) == sym, "Auto-boxed symbol is equal to that symbol");
            assert.isFalse(Object(sym) == Object(sym), "Two different auto-boxed symbols of the same symbol are never equal to each other");

            assert.isFalse(sym === Object(sym), "Auto-boxed symbol is not strict-equal to that symbol");
            assert.isFalse(Object(sym) === sym, "Auto-boxed symbol is not strict-equal to that symbol");
            assert.isFalse(Object(sym) === Object(sym), "Two different auto-boxed symbols of the same symbol are never strict-equal to each other");
        }
    },
    {
        name: "Symbol auto-boxing",
        body: function() {
            assert.areEqual('Symbol()', Symbol().toString(), "Autoboxing for toString()");

            var sym = Symbol();

            assert.isTrue(sym.valueOf() === sym.valueOf(), "Autoboxing for valueOf()");
        }
    },
    {
        name: "Symbol primitives work as property keys",
        body: function() {
            var o = {};
            o[Symbol.iterator] = 'some string';
            assert.areEqual('some string', o[Symbol.iterator], "o[Symbol.iterator] === 'some string'");
            assert.isTrue(o[Symbol.iterator.toString()] === undefined, "o[Symbol.iterator] uses the property id stored in Symbol.iterator (not the value created by Symbol.iterator.toString())");

            // use functions to wrap property access to ensure we hit JIT code
            function getProperty(obj, sym) {
                return obj[sym];
            }
            function setProperty(obj, sym, val) {
                obj[sym] = val;
            }

            o = {};
            var my = Symbol();
            for (var i = 0; i < 5; i++) {
                setProperty(o, my, i);
                assert.areEqual(i, getProperty(o, my), "Property keyed by symbol is able to be set and get");
            }

            var sym = Symbol('sym');
            o = {};
            o[sym] = 'test';

            assert.areEqual('test', o[sym], "Symbol converted to property key works");
            assert.areEqual(undefined, o['sym'], "Symbol description is not added as a property to the object");

            var s1 = Symbol('uniquevalue');
            var s2 = Symbol('uniquevalue');
            o = {};
            o[s1] = 's1';
            o[s2] = 's2';

            assert.areEqual('s1', o[s1], "Simple test of symbol producing a property on an object");
            assert.areEqual('s2', o[s2], "Simple test of symbol producing a property on an object");
            assert.isTrue(o[s1] != o[s2], "Different symbols with the same description create different properties on an object");

            delete o[s1];

            assert.areEqual(undefined, o[s1], "deleting properties from objects also works");
            assert.areEqual('s2', o[s2], "deleting a property doesn't affect other properties");

            // Needs ES6 object literal improvements
            o = { [sym] : 'string' };
            assert.areEqual('string', o[sym], "Object literal declared with symbol property works");
        }
    },
    {
        name: "Object.prototype.hasOwnProperty works for symbols",
        body: function() {
            var o = {};

            assert.isFalse(o.hasOwnProperty(Symbol.iterator), "Property defined with a symbol initially is not in the object");

            o[Symbol.iterator] = 'a string';

            assert.isTrue(o.hasOwnProperty(Symbol.iterator), "Property defined with a symbol can be looked up via o.hasOwnProperty");
        }
    },
    {
        name: "Symbols handled by type conversion operations",
        body: function() {
            assert.throws(function () { Number(Symbol.iterator).valueOf() }, TypeError, "ToNumber(symbol) throws TypeError", "Number expected");
            assert.areEqual(true, Boolean(Symbol.iterator), "ToBoolean(symbol) === true");
            assert.areEqual('object', typeof Object(Symbol.iterator), "ToObject(symbol) is not a symbol object");
        }
    },
    {
        name: "API shape of Object.getOwnPropertySymbols",
        body: function() {
            assert.isTrue(Object.getOwnPropertySymbols !== undefined, "Object.getOwnPropertySymbols is defined");
            assert.areEqual('function', typeof Object.getOwnPropertySymbols, "Object.getOwnPropertySymbols is a function");
            assert.areEqual(1, Object.getOwnPropertySymbols.length, "Object.getOwnPropertySymbols.length === 1");
        }
    },
    {
        name: "Object.getOwnPropertySymbols does ToObject on its argument",
        body: function() {
            assert.throws(function () { Object.getOwnPropertySymbols(); }, TypeError, "ToObject(undefined) throws TypeError", "Object expected");
            assert.throws(function () { Object.getOwnPropertySymbols(undefined); }, TypeError, "ToObject(undefined) throws TypeError", "Object expected");
            assert.throws(function () { Object.getOwnPropertySymbols(null); }, TypeError, "ToObject(null) throws TypeError", "Object expected");
            assert.areEqual([], Object.getOwnPropertySymbols(true), "Object.getOwnPropertySymbols does ToObject on boolean");
            assert.areEqual([], Object.getOwnPropertySymbols(1), "Object.getOwnPropertySymbols does ToObject on number");
            assert.areEqual([], Object.getOwnPropertySymbols("a"), "Object.getOwnPropertySymbols does ToObject on string");
            assert.areEqual([], Object.getOwnPropertySymbols(Symbol('a')), "Object.getOwnPropertySymbols does ToObject on symbol");
            assert.areEqual([], Object.getOwnPropertySymbols({}), "Object.getOwnPropertySymbols returns an empty array for an empty object");
        }
    },
    {
        name: "Object.getOwnPropertySymbols only returns symbols",
        body: function() {
            var sym = Symbol('c');
            var o = {};

            o['a'] = 'alpha';
            Object.defineProperty(o, 'b', { value: 'beta', enumerable: false } );
            o[sym] = 'gamma';
            o['d'] = 'delta';

            var symbols = Object.getOwnPropertySymbols(o);

            assert.areEqual(1, symbols.length, "symbols.length === 1");

            for(var i = 0; i < symbols.length; i++) {
                assert.isTrue(typeof symbols[i] === 'symbol', "The symbols array only includes entries of type symbol");
                assert.isTrue(symbols[i].toString() != 'a', "The symbols array does not include an entry with the name of any of our string properties");
                assert.isTrue(symbols[i].toString() != 'b', "The symbols array does not include an entry with the name of any of our string properties");
                assert.isTrue(symbols[i].toString() != 'd', "The symbols array does not include an entry with the name of any of our string properties");
                assert.isTrue(symbols[i] === sym, "The symbols array includes our symbol");
                assert.isTrue(symbols[i].toString() === sym.toString(), "The symbols array includes an entry with our symbol.toString()");
            }

            var s1 = Symbol('name');
            var s2 = Symbol('name');
            o = {};

            o[s1] = 'something';
            o[s2] = 'something else';

            symbols = Object.getOwnPropertySymbols(o);

            assert.areEqual(2, symbols.length, "symbols.length === 2");
            assert.isTrue(symbols[0] === s1, "symbols[0] === s1");
            assert.isTrue(symbols[1] === s2, "symbols[1] === s2");

            o = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

            symbols = Object.getOwnPropertySymbols(o);

            assert.areEqual(0, symbols.length, "Object with no symbol properties returns empty array from Object.getOwnPropertySymbols");
        }
    },
    {
        name: "Object.getOwnPropertyNames doesn't return symbols",
        body: function() {
            var sym = Symbol('c');
            var o = {};

            o['a'] = 'alpha';
            Object.defineProperty(o, 'b', { value: 'beta', enumerable: false } );
            o[sym] = 'gamma';
            o['d'] = 'delta';

            var names = Object.getOwnPropertyNames(o);

            assert.areEqual(3, names.length, "names.length === 3");

            for(var i = 0; i < names.length; i++) {
                assert.isFalse(typeof names[i] === 'symbol', "The names array does not include an entry of type symbol");
                assert.isTrue(names[i] != 'c', "The names array does not include an entry with the description of our symbol");
                assert.isTrue(names[i] != sym, "The names array does not include any symbols");
                assert.isTrue(names[i] != sym.toString(), "The names array does not include an entry with our symbol.toString()");
            }

            o = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

            names = Object.getOwnPropertyNames(o);

            assert.areEqual(11, names.length, "Object with no symbol properties returns correct array");

            o = {};

            o[sym] = 'something';

            names = Object.getOwnPropertyNames(o);

            assert.areEqual(0, names.length, "Object with only symbol properties returns empty array");
        }
    },
    {
        name: "Object.keys should not return property keys which are symbols",
        body: function() {
            var sym = Symbol('c');
            var o = {};

            o['a'] = 'alpha';
            o['b'] = 'beta';
            o[sym] = 'gamma';
            o['d'] = 'delta';

            var keys = Object.keys(o);

            assert.areEqual(3, keys.length, "keys.length === 3");

            for(var i = 0; i < keys.length; i++) {
                assert.isFalse(typeof keys[i] === 'symbol', "The keys array does not include an entry of type symbol");
                assert.isTrue(keys[i] != 'c', "The keys array does not include an entry with the description of our symbol");
                assert.isTrue(keys[i] != sym, "The keys array does not include any symbols");
                assert.isTrue(keys[i] != sym.toString(), "The keys array does not include an entry with our symbol.toString()");
            }
        }
    },
    {
        name: "for ... in enumeration does not surface symbols",
        body: function() {
            var sym = Symbol('c');
            var o = {};

            o['a'] = 'alpha';
            o['b'] = 'beta';
            o[sym] = 'gamma';
            o['d'] = 'delta';

            for (k in o)
            {
                assert.isFalse(typeof k === 'symbol', "for ... in does not enumerate symbol types");
                assert.isTrue(o[k] !== 'gamma', "for ... in does not enumerate properties keyed by symbols");
                assert.isTrue(k != sym, "for ... in does not enumerate our symbol");
                assert.isTrue(k != sym.toString(), "for ... in does not enumerate a property named sym.toString()");
            }
        }
    },
    {
        name: "Object.defineProperty with symbol as property key",
        body: function() {
            var sym = Symbol();
            var o = {};

            Object.defineProperty(o, sym, { value: 'some value' } );

            assert.areEqual('some value', o[sym], "Property keyed off symbol and set via Object.defineProperty should be reachable by the same symbol");
            assert.areEqual(undefined, o['sym'], "defineProperty does not create a propery based on symbol name");
            assert.areEqual(undefined, o[''], "defineProperty does not create a propery based on symbol description");
            assert.areEqual(undefined, o[sym.toString()], "defineProperty does not create a propery based on symbol toString() value");
        }
    },
    {
        name: "Object.defineProperties with symbols as property keys",
        body: function() {
            var props = {};
            var s1 = Symbol('symbol 1');
            var s2 = Symbol('symbol 2');
            props['a'] = { value: 'alpha', enumerable: true };
            props[s1] = { value: 'beta', enumerable: true };
            props[s2] = { value: 'gamma', enumerable: true };
            props['d'] = { value: 'delta', enumerable: true };
            var o = {};

            Object.defineProperties(o, props);

            assert.areEqual('alpha', o['a'], "Property keyed off string is added as expected");
            assert.areEqual('delta', o['d'], "Property keyed off string is added as expected");

            assert.areEqual('beta', o[s1], "Property keyed off symbol set via Object.defineProperties should be reachable by the same symbol");
            assert.areEqual('gamma', o[s2], "Property keyed off symbol set via Object.defineProperties should be reachable by the same symbol");
            assert.areEqual(undefined, o['s1'], "defineProperties does not create a property based on symbol name");
            assert.areEqual(undefined, o['s2'], "defineProperties does not create a property based on symbol name");
            assert.areEqual(undefined, o['symbol 1'], "defineProperties does not create a property based on symbol description");
            assert.areEqual(undefined, o['symbol 2'], "defineProperties does not create a property based on symbol description");
            assert.areEqual(undefined, o[s1.toString()], "defineProperty does not create a propery based on symbol toString() value");
            assert.areEqual(undefined, o[s2.toString()], "defineProperty does not create a propery based on symbol toString() value");
        }
    },
    {
        name: "Object.create should work for symbol properties",
        body: function() {
            var props = {};
            var s1 = Symbol('symbol 1');
            var s2 = Symbol('symbol 2');
            props['a'] = { value: 'alpha', enumerable: true };
            props[s1] = { value: 'beta', enumerable: true };
            props[s2] = { value: 'gamma', enumerable: true };
            props['d'] = { value: 'delta', enumerable: true };

            var o = Object.create(Object.prototype, props);

            assert.areEqual('alpha', o['a'], "Property keyed off string is added as expected");
            assert.areEqual('delta', o['d'], "Property keyed off string is added as expected");

            assert.areEqual('beta', o[s1], "Property keyed off symbol set via Object.create should be reachable by the same symbol");
            assert.areEqual('gamma', o[s2], "Property keyed off symbol set via Object.create should be reachable by the same symbol");
            assert.areEqual(undefined, o['s1'], "Object.create does not create a property based on symbol name");
            assert.areEqual(undefined, o['s2'], "Object.create does not create a property based on symbol name");
            assert.areEqual(undefined, o['symbol 1'], "Object.create does not create a property based on symbol description");
            assert.areEqual(undefined, o['symbol 2'], "Object.create does not create a property based on symbol description");
            assert.areEqual(undefined, o[s1.toString()], "Object.create does not create a propery based on symbol toString() value");
            assert.areEqual(undefined, o[s2.toString()], "Object.create does not create a propery based on symbol toString() value");
        }
    },
    {
        name: "Object.getOwnPropertyDescriptor with symbol as property key",
        body: function() {
            var sym = Symbol();
            var o = {};

            Object.defineProperty(o, sym, { value: 100000, writable: false, enumerable: true, configurable: false } );
            var descriptor = Object.getOwnPropertyDescriptor(o, sym);

            assert.isFalse(descriptor.writable, 'o[sym].descriptor.writable == false');
            assert.isTrue(descriptor.enumerable, 'o[sym].descriptor.enumerable == true');
            assert.isFalse(descriptor.configurable, 'o[sym].descriptor.configurable == false');
        }
    },
    {
        name: "Object.prototype.propertyIsEnumerable should work for symbol properties",
        body: function() {
            var sym1 = Symbol();
            var sym2 = Symbol();
            var o = {};
            Object.defineProperty(o, sym1, { value: 10, enumerable: true});
            Object.defineProperty(o, sym2, { value: 10, enumerable: false});

            assert.isTrue(o.propertyIsEnumerable(sym1), 'o.propertyIsEnumerable[sym1]');
            assert.isFalse(o.propertyIsEnumerable(sym2), 'o.propertyIsEnumerable[sym2]');
        }
    },
    {
        name: "Object.prototype.__defineSetter__ with a property keyed by a symbol",
        body: function() {
            var sym = Symbol();
            var o = {};
            var helpme;

            o.__defineSetter__(sym, function() { helpme = 'useful string'; });

            o[sym] = 'anything';

            assert.areEqual('useful string', helpme, "Object.prototype.__defineSetter__ works when we use a symbol");
        }
    },
    {
        name: "Object.prototype.__defineGetter__ with a property keyed by a symbol",
        body: function() {
            var sym = Symbol();
            var o = {};

            o.__defineGetter__(sym, function() { return 'anything'; });

            assert.areEqual('anything', o[sym], "Object.prototype.__defineGetter__ works when we use a symbol");
        }
    },
    {
        name: "Object.prototype.__lookupSetter__ with a property keyed by a symbol",
        body: function() {
            var sym = Symbol();
            var o = {};
            var helpme;
            var setter = function() { helpme = 'useful string'; };

            o.__defineSetter__(sym, setter);
            var f = o.__lookupSetter__(sym);

            assert.areEqual(undefined, helpme, "setter has not yet been called");
            assert.isTrue(f === setter, "Object.prototype.__lookupSetter__ returns correct function when we use a symbol");

            f();

            assert.areEqual('useful string', helpme, "calling setter returned from Object.prototype.__lookupSetter__ works as expected");

            helpme = undefined;

            o[sym] = 'anything';

            assert.areEqual('useful string', helpme, "Object.prototype.__lookupSetter__ works when we use a symbol");
        }
    },
    {
        name: "Object.prototype.__lookupGetter__ with a property keyed by a symbol",
        body: function() {
            var sym = Symbol();
            var o = {};
            var getter = function() { return 'anything'; };

            o.__defineGetter__(sym, getter);
            var f = o.__lookupGetter__(sym);

            assert.isTrue(f === getter, "Object.prototype.__lookupGetter__ returns correct function when we use a symbol");
            assert.areEqual('anything', f(), "function returned via Object.prototype.__lookupGetter__ works as expected");
            assert.areEqual('anything', o[sym], "Object.prototype.__lookupGetter__ works when we use a symbol");
        }
    },
    {
        name: 'Symbol with numeric description does not create a numeric property',
        body: function() {
            var sym = Symbol('1');
            var o = {};

            o[sym] = 'a string';

            assert.areEqual(undefined, o[1], "Object should not contain numeric property at index == symbol description");
            assert.areEqual('a string', o[sym], "Object should contain the symbol property");

            o = [];

            o[1] = 'the number 1';
            o[sym] = 'the symbol 1';

            assert.areEqual(2, o.length, "Object has correct length");
            assert.areEqual('the number 1', o[1], "Object with numeric property has correct value");
            assert.areEqual('the symbol 1', o[sym], "Object with symbol property has correct value");
        }
    },
    {
        name: 'BLUE: 539472 BLUE: 541467 - Symbol.prototype should be TypeIds_Object',
        body: function() {
            assert.throws(function () { Symbol.prototype.valueOf(); }, TypeError, "Calling prototype methods directly fails since Symbol.prototype is not a SymbolObject", "Symbol.prototype.valueOf: 'this' is not a Symbol object");
            assert.throws(function () { Symbol.prototype.toString(); }, TypeError, "Calling prototype methods directly fails since Symbol.prototype is not a SymbolObject", "Symbol.prototype.toString: 'this' is not a Symbol object");
        }
    },
    {
        name: 'Symbol objects and properties passed cross-context',
        body: function() {
            var child = WScript.LoadScriptFile("ES6Symbol_cross_context_child.js", "samethread");

            assert.isFalse(Symbol('child symbol') === child.sym, "Symbol created in another context does not equal symbol with same description from this context");
            assert.areEqual('symbol', typeof child.sym, "Symbol created in another context has correct type");
            assert.areEqual(undefined, child.o[Symbol('child symbol')], "Object from another context with a symbol-keyed property doesn't contain a property named the same as a different symbol");
            assert.areEqual('Symbol(child symbol)', child.sym.toString(), "Symbol from another context has correct toString behavior");
            assert.areEqual('child value', child.o[child.sym], "Symbol from another context can be used to lookup properties in objects from another context");

            var o = {};
            o[child.sym] = 'parent value';

            assert.areEqual('parent value', o[child.sym], "Symbol from another context can be used to index objects from this context");

            var symbols = Object.getOwnPropertySymbols(child.o);

            assert.areEqual(1, symbols.length, "Object.getOwnPropertySymbols works for objects from another context");
            assert.isTrue(symbols[0] === child.sym, "Object.getOwnPropertySymbols returns the correct symbols for objects from another context");
        }
    },
    {
        name: 'Symbol registration within a single realm',
        body: function() {
            var sym = Symbol.for('my string');
            var sym2 = Symbol.for('my string');

            assert.areEqual('symbol', typeof sym, "Object returned from Symbol.for is actually a symbol");
            assert.areEqual('Symbol(my string)', sym.toString(), "Symbol returned from Symbol.for has the right description");
            assert.isTrue(sym === sym2, "Two symbols returned from Symbol.for with the same parameter are the same symbol");

            var key = Symbol.keyFor(sym);

            assert.areEqual('my string', key, "Symbol created by Symbol.for can be passed to Symbol.keyFor to return the same key");
        }
    },
    {
        name: 'Symbol registration cross-realm',
        body: function() {
            var parent_sym = Symbol.for('parent symbol');

            var child = WScript.LoadScriptFile("ES6Symbol_cross_context_registration_child.js", "samethread");

            var child_sym = Symbol.for('child symbol');

            assert.isTrue(child.child_sym === child_sym, "Symbol registered in child is returned correctly in parent");
            assert.isTrue(child.parent_sym === parent_sym, "Symbol registered in parent is returned correctly in child");
            assert.isTrue(child.parent_string === Symbol.keyFor(parent_sym), "Symbol registered in parent is returned correctly in child");
        }
    },
    {
        name: 'Registered Symbols should have their PropertyRecords pinned',
        body: function() {
            var sym = Symbol.for('my string');
            sym = undefined;

            // After cleaning up sym, there shouldn't be anyone pinning the PropertyRecord
            // except for the Symbol registration map.
            // If the reference to the PropertyRecord created above gets cleaned-up we will
            // cause an AV below when we try to reference it again.
            CollectGarbage();

            sym = Symbol.for('my string');

            assert.areEqual('symbol', typeof sym, "Object returned from Symbol.for is actually a symbol");
            assert.areEqual('Symbol(my string)', sym.toString(), "Symbol returned from Symbol.for has the right description");
        }
    },
    {
        name: 'Registered Symbol should not be returned by unregistered Symbol with the same description',
        body: function() {
            var sym = Symbol.for('my string');
            var sym2 = Symbol('my string');

            assert.isFalse(sym === sym2, "Symbols created via Symbol.for and Symbol constructor should not be equal even if they have the same description");
            assert.areEqual('my string', Symbol.keyFor(sym), "Symbol.keyFor returns correct key for registered symbol");
            assert.areEqual(undefined, Symbol.keyFor(sym2), "Symbol.keyFor returns undefined for symbols not registered with Symbol.for");
        }
    },
    {
        name: 'Throwing TypeError when trying to add with a string or a number',
        body: function() {
            var x = Symbol();

            assert.throws(function() { "str" + x; }, TypeError, "Adding a string and a symbol throws TypeError", "Object doesn't support property or method 'ToString'");
            assert.throws(function() { x + "str"; }, TypeError, "Adding a symbol and a string throws TypeError", "Object doesn't support property or method 'ToString'");
            assert.throws(function() { 10 + x; }, TypeError, "Adding a number and a symbol throws TypeError", "Number expected");
            assert.throws(function() { x + 10; }, TypeError, "Adding a symbol and a number throws TypeError", "Number expected");
        }
    },
    {
        name: 'ToPropertyKey accepts Symbol wrapper objects, and unboxes the Symbol primitive inside',
        body: function() {
            var sym = Symbol('sym');
            var sym_object = Object(sym);
            var obj = { [sym_object] : 'value' };

            assert.areEqual('value', obj[sym], "Object created with Symbol wrapper object passed as computed property creates a symbol-keyed property from the unboxed symbol");
            assert.areEqual('value', obj[sym_object], "Looking up a property by passing a Symbol wrapper object actually returns the property keyed off of the unboxed symbol");

            assert.areEqual([], Object.getOwnPropertyNames(obj), "Object has no string-keyed properties");
            assert.areEqual([sym], Object.getOwnPropertySymbols(obj), "Object only has one symbol-keyed property - sym");

            var obj2 = {};
            obj2[sym_object] = 'value2';

            assert.areEqual('value2', obj2[sym], "Object created with Symbol wrapper object passed to property index set creates a symbol-keyed property from the unboxed symbol");
            assert.areEqual('value2', obj2[sym_object], "Looking up a property by passing a Symbol wrapper object actually returns the property keyed off of the unboxed symbol");

            assert.areEqual([], Object.getOwnPropertyNames(obj2), "Object has no string-keyed properties");
            assert.areEqual([sym], Object.getOwnPropertySymbols(obj2), "Object only has one symbol-keyed property - sym");
        }
    },
    {
        name: 'ToPropertyKey performs ToPrimitive on argument which unwraps Symbol objects',
        body: function() {
            var sym = Symbol('sym');
            var symbol_object = Object(sym);

            VerfiyToPropertyKey(symbol_object);
        }
    },
    {
        name: 'ToPropertyKey performs ToPrimitive on argument which returns symbol primitive via toString',
        body: function() {
            var sym = Symbol('sym');
            var tostring_object = {
                toString() {
                    return sym;
                },
                valueOf() {
                    assert.isTrue(false, "We should never call the valueOf method of this object");
                }
            };

            VerfiyToPropertyKey(tostring_object);
        }
    },
    {
        name: 'ToPropertyKey performs ToPrimitive on argument which returns symbol primitive via valueOf',
        body: function() {
            var sym = Symbol('sym');
            var obj = { [sym] : 'value' };
            var valueof_object = {
                toString : null,
                valueOf() {
                    return sym;
                }
            };

            VerfiyToPropertyKey(valueof_object);
        }
    },
    {
        name: 'ToNumber called with a symbol primitive should throw a TypeError',
        body: function() {
            var x = Symbol();
            var obj = { 'length': x };

            // We can't use parseInt directly here as that does ToString(obj) - we want something which calls ToNumber directly
            assert.throws(function() { Array.prototype.lastIndexOf.call(obj, 1); }, TypeError, "Array.prototype.lastIndexOf performs ToLength(obj) which should throw TypeError if obj is a symbol primitive.", "Number expected");
        }
    },
    {
        name: 'Assigning to a property of a symbol primitive in strict mode should throw a TypeError',
        body: function() {
            var x = Symbol();
            assert.throws(function() { "use strict"; x.a = 1; }, TypeError, "Assigning to a property of a symbol primitive should throw a TypeError.", "Assignment to read-only properties is not allowed in strict mode");
        }
    },
    {
        name: 'Assigning to a property of a symbol primitive in strict mode should throw a TypeError',
        body: function() {
            var x = Symbol();
            assert.throws(function() { "use strict"; x['a'+'b'] = 1; }, TypeError, "Assigning to a property of a symbol primitive should throw a TypeError.", "Assignment to read-only properties is not allowed in strict mode");
        }
    },
    {
        name: 'Assigning to an index of a symbol primitive in strict mode should throw a TypeError',
        body: function() {
            var x = Symbol();
            assert.throws(function() { "use strict"; x[12] = 1; }, TypeError, "Assigning to an index of a symbol primitive should throw a TypeError.", "Assignment to read-only properties is not allowed in strict mode");
        }
    },
    {
        name: 'Assigning to a property of a symbol primitive should be ignored',
        body: function() {
            var x = Symbol();
            x.a = 1;
            assert.areEqual(x.a, undefined);
        }
    },
    {
        name: 'Assigning to a property of a symbol primitive should be ignored',
        body: function() {
            var x = Symbol();
            x['a'+'b'] = 1;
            assert.areEqual(x['ab'], undefined);
        }
    },
    {
        name: 'Assigning to an index of a symbol primitive should be ignored',
        body: function() {
            var x = Symbol();
            x[10086] = 1;
            assert.areEqual(x[10086], undefined);
        }
    },
    {
        name: '[OS: Bug 4533235] JSON.stringify should ignore symbols (kangax)',
        body: function() {
            var object = {foo: Symbol()};
            var sym = Symbol("a");
            object[Symbol()] = 1;
            var array = [Symbol()];

            assert.areEqual('{}', JSON.stringify(object));
            assert.areEqual('[null]', JSON.stringify(array));
            assert.areEqual(undefined, JSON.stringify(Symbol()));
            assert.areEqual(undefined, JSON.stringify(sym));
        }
    }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
