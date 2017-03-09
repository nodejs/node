/* eslint-disable strict */
const common = require('../common');
const assert = require('assert');
const vm = require('vm');

common.globalCheck = false;

// Run a string
const result = vm.runInThisContext('\'passed\';');
assert.strictEqual('passed', result);

// thrown error
assert.throws(function() {
  vm.runInThisContext('throw new Error(\'test\');');
}, /test/);

global.hello = 5;
vm.runInThisContext('hello = 2');
assert.strictEqual(2, global.hello);


// pass values
const code = 'foo = 1;' +
             'bar = 2;' +
             'if (typeof baz !== \'undefined\') ' +
             'throw new Error(\'test fail\');';
global.foo = 2;
global.obj = { foo: 0, baz: 3 };
/* eslint-disable no-unused-vars */
const baz = vm.runInThisContext(code);
/* eslint-enable no-unused-vars */
assert.strictEqual(0, global.obj.foo);
assert.strictEqual(2, global.bar);
assert.strictEqual(1, global.foo);

// call a function
global.f = function() { global.foo = 100; };
vm.runInThisContext('f()');
assert.strictEqual(100, global.foo);
