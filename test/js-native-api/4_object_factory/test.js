'use strict';
// Addons: 4_object_factory, 4_object_factory_vtable

const { addonPath } = require('../../common/addon-test');
const assert = require('assert');
const addon = require(addonPath);

const obj1 = addon('hello');
const obj2 = addon('world');
assert.strictEqual(`${obj1.msg} ${obj2.msg}`, 'hello world');
