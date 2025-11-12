'use strict';

// This test verifies that C++ static variable dynamic initialization is called
// correctly and does not interfere with the module initialization.
const common = require('../../common');
const test_init_order = require(`./build/${common.buildType}/test_init_order`);
const assert = require('assert');

assert.strictEqual(test_init_order.cppIntValue, 42);
assert.strictEqual(test_init_order.cppStringValue, '123');
