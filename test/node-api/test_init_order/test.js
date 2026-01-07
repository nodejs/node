'use strict';
// Addons: test_init_order, test_init_order_vtable

// This test verifies that C++ static variable dynamic initialization is called
// correctly and does not interfere with the module initialization.

const { addonPath } = require('../../common/addon-test');
const test_init_order = require(addonPath);
const assert = require('assert');

assert.strictEqual(test_init_order.cppIntValue, 42);
assert.strictEqual(test_init_order.cppStringValue, '123');
