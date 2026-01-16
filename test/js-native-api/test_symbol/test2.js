'use strict';
// Addons: test_symbol, test_symbol_vtable

const { addonPath } = require('../../common/addon-test');
const assert = require('assert');

// Testing api calls for symbol
const test_symbol = require(addonPath);

const fooSym = test_symbol.New('foo');
assert.strictEqual(fooSym.toString(), 'Symbol(foo)');

const myObj = {};
myObj.foo = 'bar';
myObj[fooSym] = 'baz';

assert.deepStrictEqual(Object.keys(myObj), ['foo']);
assert.deepStrictEqual(Object.getOwnPropertyNames(myObj), ['foo']);
assert.deepStrictEqual(Object.getOwnPropertySymbols(myObj), [fooSym]);
