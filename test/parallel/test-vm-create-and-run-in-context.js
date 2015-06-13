'use strict';
var common = require('../common');
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
