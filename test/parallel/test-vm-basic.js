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
assert.deepEqual(sandbox, {
  foo: 'bar',
  typeofProcess: 'undefined',
});
assert.strictEqual(result, 'function');

// Test 2: vm.runInContext
var sandbox2 = { foo: 'bar' };
var context = vm.createContext(sandbox2);
var result = vm.runInContext(
  'baz = foo; this.typeofProcess = typeof process; typeof Object;',
  context
);
assert.deepEqual(sandbox2, {
  foo: 'bar',
  baz: 'bar',
  typeofProcess: 'undefined'
});
assert.strictEqual(result, 'function');

// Test 3: vm.runInThisContext
var result = vm.runInThisContext(
  'vmResult = "foo"; Object.prototype.toString.call(process);'
);
assert.strictEqual(global.vmResult, 'foo');
assert.strictEqual(result, '[object process]');

// Test 4: vm.runInGlobalContext
var result = vm.runInModuleContext(`
  const path = require('path')
  vmResult = path.dirname('/foo/bar/baz/asdf/quux');
  Object.prototype.toString.call(process);`
);
assert.strictEqual(global.vmResult,
  require('path').dirname('/foo/bar/baz/asdf/quux'));
delete global.vmResult;

// Test 5: vm.runInNewContext
var result = vm.runInNewContext(
  'vmResult = "foo"; typeof process;'
);
assert.strictEqual(global.vmResult, undefined);
assert.strictEqual(result, 'undefined');

// Test 6: vm.createContext
var sandbox3 = {};
var context2 = vm.createContext(sandbox3);
assert.strictEqual(sandbox3, context2);
