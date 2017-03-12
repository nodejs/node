'use strict';
// Flags: --expose-gc

const common = require('../common');
const assert = require('assert');
const vm = require('vm');

assert.strictEqual(typeof global.gc, 'function',
                   'Run this test with --expose-gc');

common.globalCheck = false;

console.error('run a string');
const result = vm.runInNewContext('\'passed\';');
assert.strictEqual('passed', result);

console.error('thrown error');
assert.throws(function() {
  vm.runInNewContext('throw new Error(\'test\');');
}, /^Error: test$/);

global.hello = 5;
vm.runInNewContext('hello = 2');
assert.strictEqual(5, global.hello);


console.error('pass values in and out');
global.code = 'foo = 1;' +
              'bar = 2;' +
              'if (baz !== 3) throw new Error(\'test fail\');';
global.foo = 2;
global.obj = { foo: 0, baz: 3 };
/* eslint-disable no-unused-vars */
const baz = vm.runInNewContext(global.code, global.obj);
/* eslint-enable no-unused-vars */
assert.strictEqual(1, global.obj.foo);
assert.strictEqual(2, global.obj.bar);
assert.strictEqual(2, global.foo);

console.error('call a function by reference');
function changeFoo() { global.foo = 100; }
vm.runInNewContext('f()', { f: changeFoo });
assert.strictEqual(global.foo, 100);

console.error('modify an object by reference');
const f = { a: 1 };
vm.runInNewContext('f.a = 2', { f: f });
assert.strictEqual(f.a, 2);

console.error('use function in context without referencing context');
const fn = vm.runInNewContext('(function() { obj.p = {}; })', { obj: {} });
global.gc();
fn();
// Should not crash
