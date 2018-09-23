/* eslint-disable strict */
var common = require('../common');
var assert = require('assert');
var vm = require('vm');

common.globalCheck = false;

console.error('run a string');
var result = vm.runInThisContext('\'passed\';');
assert.equal('passed', result);

console.error('thrown error');
assert.throws(function() {
  vm.runInThisContext('throw new Error(\'test\');');
}, /test/);

hello = 5;
vm.runInThisContext('hello = 2');
assert.equal(2, hello);


console.error('pass values');
code = 'foo = 1;' +
       'bar = 2;' +
       'if (typeof baz !== \'undefined\') throw new Error(\'test fail\');';
foo = 2;
obj = { foo: 0, baz: 3 };
var baz = vm.runInThisContext(code);
assert.equal(0, obj.foo);
assert.equal(2, bar);
assert.equal(1, foo);

console.error('call a function');
f = function() { foo = 100; };
vm.runInThisContext('f()');
assert.equal(100, foo);
