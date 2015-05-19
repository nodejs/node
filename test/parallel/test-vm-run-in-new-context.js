/* eslint-disable strict */
// Flags: --expose-gc

var common = require('../common');
var assert = require('assert');
var vm = require('vm');

assert.equal(typeof gc, 'function', 'Run this test with --expose-gc');

common.globalCheck = false;

console.error('run a string');
var result = vm.runInNewContext('\'passed\';');
assert.equal('passed', result);

console.error('thrown error');
assert.throws(function() {
  vm.runInNewContext('throw new Error(\'test\');');
});

hello = 5;
vm.runInNewContext('hello = 2');
assert.equal(5, hello);


console.error('pass values in and out');
code = 'foo = 1;' +
       'bar = 2;' +
       'if (baz !== 3) throw new Error(\'test fail\');';
foo = 2;
obj = { foo: 0, baz: 3 };
var baz = vm.runInNewContext(code, obj);
assert.equal(1, obj.foo);
assert.equal(2, obj.bar);
assert.equal(2, foo);

console.error('call a function by reference');
function changeFoo() { foo = 100; }
vm.runInNewContext('f()', { f: changeFoo });
assert.equal(foo, 100);

console.error('modify an object by reference');
var f = { a: 1 };
vm.runInNewContext('f.a = 2', { f: f });
assert.equal(f.a, 2);

console.error('use function in context without referencing context');
var fn = vm.runInNewContext('(function() { obj.p = {}; })', { obj: {} });
gc();
fn();
// Should not crash
