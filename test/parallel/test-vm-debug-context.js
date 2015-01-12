var common = require('../common');
var assert = require('assert');
var vm = require('vm');

assert.throws(function() {
  vm.runInDebugContext('*');
}, /SyntaxError/);

assert.throws(function() {
  vm.runInDebugContext({ toString: assert.fail });
}, /AssertionError/);

assert.throws(function() {
  vm.runInDebugContext('throw URIError("BAM")');
}, /URIError/);

assert.throws(function() {
  vm.runInDebugContext('(function(f) { f(f) })(function(f) { f(f) })');
}, /RangeError/);

assert.equal(typeof(vm.runInDebugContext('this')), 'object');
assert.equal(typeof(vm.runInDebugContext('Debug')), 'object');

assert.strictEqual(vm.runInDebugContext(), undefined);
assert.strictEqual(vm.runInDebugContext(0), 0);
assert.strictEqual(vm.runInDebugContext(null), null);
assert.strictEqual(vm.runInDebugContext(undefined), undefined);
