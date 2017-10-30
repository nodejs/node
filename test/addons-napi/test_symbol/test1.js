'use strict';
const common = require('../../common');
const assert = require('assert');

// testing api calls for symbol
const test_symbol = require(`./build/${common.buildType}/test_symbol`);

const sym = test_symbol.New('test');
assert.strictEqual(sym.toString(), 'Symbol(test)');


const myObj = {};
const fooSym = test_symbol.New('foo');
const otherSym = test_symbol.New('bar');
myObj['foo'] = 'bar';
myObj[fooSym] = 'baz';
myObj[otherSym] = 'bing';
assert.strictEqual(myObj.foo, 'bar');
assert.strictEqual(myObj[fooSym], 'baz');
assert.strictEqual(myObj[otherSym], 'bing');
