'use strict';
// Flags: --expose-gc
require('../common');
var assert = require('assert');

var vm = require('vm');

console.error('run in a new empty context');
var context = vm.createContext();
var result = vm.runInContext('"passed";', context);
assert.equal('passed', result);

console.error('create a new pre-populated context');
context = vm.createContext({'foo': 'bar', 'thing': 'lala'});
assert.equal('bar', context.foo);
assert.equal('lala', context.thing);

console.error('test updating context');
result = vm.runInContext('var foo = 3;', context);
assert.equal(3, context.foo);
assert.equal('lala', context.thing);

// https://github.com/nodejs/node/issues/5768
console.error('run in contextified sandbox without referencing the context');
var sandbox = {x: 1};
vm.createContext(sandbox);
global.gc();
vm.runInContext('x = 2', sandbox);
// Should not crash.
