// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
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
delete global.vmResult;

// Test 4: vm.runInNewContext
var result = vm.runInNewContext(
  'vmResult = "foo"; typeof process;'
);
assert.strictEqual(global.vmResult, undefined);
assert.strictEqual(result, 'undefined');

// Test 5: vm.createContext
var sandbox3 = {};
var context2 = vm.createContext(sandbox3);
assert.strictEqual(sandbox3, context2);
