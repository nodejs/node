'use strict';
// Addons: 5_function_factory, 5_function_factory_vtable

const { addonPath } = require('../../common/addon-test');
const assert = require('assert');
const addon = require(addonPath);

const fn = addon();
assert.strictEqual(fn(), 'hello world'); // 'hello world'
