'use strict';
const common = require('../../common');
const assert = require('assert');
const addon = require(`./build/${common.buildType}/binding`);

const obj1 = addon.createObject(10);
const obj2 = addon.createObject(20);
const result = addon.add(obj1, obj2);
assert.strictEqual(result, 30);
