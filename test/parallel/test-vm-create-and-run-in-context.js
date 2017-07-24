'use strict';
// Flags: --expose-gc
require('../common');
const assert = require('assert');

const vm = require('vm');

// Run in a new empty context
let context = vm.createContext();
let result = vm.runInContext('"passed";', context);
assert.strictEqual('passed', result);

// Create a new pre-populated context
context = vm.createContext({ 'foo': 'bar', 'thing': 'lala' });
assert.strictEqual('bar', context.foo);
assert.strictEqual('lala', context.thing);

// Test updating context
result = vm.runInContext('var foo = 3;', context);
assert.strictEqual(3, context.foo);
assert.strictEqual('lala', context.thing);

// https://github.com/nodejs/node/issues/5768
// Run in contextified sandbox without referencing the context
const sandbox = { x: 1 };
vm.createContext(sandbox);
global.gc();
vm.runInContext('x = 2', sandbox);
// Should not crash.
