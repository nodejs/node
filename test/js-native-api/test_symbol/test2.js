'use strict';
const common = require('../../common');
const assert = require('assert');

// Testing api calls for symbol
const test_symbol = require(`./build/${common.buildType}/test_symbol`);

const fooSym = test_symbol.New('foo');
assert.strictEqual(fooSym.toString(), 'Symbol(foo)');

const myObj = {};
myObj.foo = 'bar';
myObj[fooSym] = 'baz';

assert.deepStrictEqual(Object.keys(myObj), ['foo']);
assert.deepStrictEqual(Object.getOwnPropertyNames(myObj), ['foo']);
assert.deepStrictEqual(Object.getOwnPropertySymbols(myObj), [fooSym]);
