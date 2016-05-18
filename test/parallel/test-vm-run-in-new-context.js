'use strict';
// Flags: --expose-gc

const common = require('../common');
const assert = require('assert');
const vm = require('vm');

assert.equal(typeof global.gc, 'function', 'Run this test with --expose-gc');

common.globalCheck = false;

console.error('run a string');
const result = vm.runInNewContext('\'passed\';');
assert.equal('passed', result);

console.error('thrown error');
assert.throws(function() {
  vm.runInNewContext('throw new Error(\'test\');');
});

global.hello = 5;
vm.runInNewContext('hello = 2');
assert.equal(5, global.hello);


console.error('pass values in and out');
global.code = 'foo = 1;' +
              'bar = 2;' +
              'if (baz !== 3) throw new Error(\'test fail\');';
global.foo = 2;
global.obj = { foo: 0, baz: 3 };
/* eslint-disable no-unused-vars */
var baz = vm.runInNewContext(global.code, global.obj);
/* eslint-enable no-unused-vars */
assert.equal(1, global.obj.foo);
assert.equal(2, global.obj.bar);
assert.equal(2, global.foo);

console.error('call a function by reference');
function changeFoo() { global.foo = 100; }
vm.runInNewContext('f()', { f: changeFoo });
assert.equal(global.foo, 100);

console.error('modify an object by reference');
var f = { a: 1 };
vm.runInNewContext('f.a = 2', { f: f });
assert.equal(f.a, 2);

console.error('use function in context without referencing context');
var fn = vm.runInNewContext('(function() { obj.p = {}; })', { obj: {} });
global.gc();
fn();
// Should not crash
