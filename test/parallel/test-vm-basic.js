'use strict';
require('../common');
var assert = require('assert');
var vm = require('vm');

// Test 1: vm.runInNewContext
var sandbox = {};
var result = vm.runInNewContext(
  'foo = "bar"; this.typeofProcess = typeof process; typeof Object;',
  sandbox
);
assert.deepStrictEqual(sandbox, {
  foo: 'bar',
  typeofProcess: 'undefined',
});
assert.strictEqual(result, 'function');

// Test 2: vm.runInContext
var sandbox2 = { foo: 'bar' };
var context = vm.createContext(sandbox2);
result = vm.runInContext(
  'baz = foo; this.typeofProcess = typeof process; typeof Object;',
  context
);
assert.deepStrictEqual(sandbox2, {
  foo: 'bar',
  baz: 'bar',
  typeofProcess: 'undefined'
});
assert.strictEqual(result, 'function');

// Test 3: vm.runInThisContext
result = vm.runInThisContext(
  'vmResult = "foo"; Object.prototype.toString.call(process);'
);
assert.strictEqual(global.vmResult, 'foo');
assert.strictEqual(result, '[object process]');
delete global.vmResult;

// Test 4: vm.runInNewContext
result = vm.runInNewContext(
  'vmResult = "foo"; typeof process;'
);
assert.strictEqual(global.vmResult, undefined);
assert.strictEqual(result, 'undefined');

// Test 5: vm.createContext
var sandbox3 = {};
var context2 = vm.createContext(sandbox3);
assert.strictEqual(sandbox3, context2);
