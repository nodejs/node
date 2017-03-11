'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

assert.throws(function() {
  vm.isContext('string is not supported');
}, TypeError);

assert.strictEqual(vm.isContext({}), false);
assert.strictEqual(vm.isContext([]), false);

assert.strictEqual(vm.isContext(vm.createContext()), true);
assert.strictEqual(vm.isContext(vm.createContext([])), true);

const sandbox = { foo: 'bar' };
vm.createContext(sandbox);
assert.strictEqual(vm.isContext(sandbox), true);
