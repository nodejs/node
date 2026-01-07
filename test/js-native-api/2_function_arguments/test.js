'use strict';
// Addons: 2_function_arguments, 2_function_arguments_vtable

const { addonPath } = require('../../common/addon-test');
const assert = require('assert');
const addon = require(addonPath);

assert.strictEqual(addon.add(3, 5), 8);
