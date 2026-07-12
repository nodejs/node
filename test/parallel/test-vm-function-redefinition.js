'use strict';
// Refs: https://github.com/nodejs/node/issues/548
require('../common');
const assert = require('assert');
const vm = require('vm');
const context = vm.createContext();

vm.runInContext('function test() { return 0; }', context);
vm.runInContext('function test() { return 1; }', context);
const result = vm.runInContext('test()', context);
assert.strictEqual(result, 1);
