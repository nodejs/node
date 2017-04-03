'use strict';
const common = require('../../common');
const assert = require('assert');
const createObject = require(`./build/${common.buildType}/binding`);

const obj = createObject(10);
assert.strictEqual(obj.plusOne(), 11);
assert.strictEqual(obj.plusOne(), 12);
assert.strictEqual(obj.plusOne(), 13);

const obj2 = createObject(20);
assert.strictEqual(obj2.plusOne(), 21);
assert.strictEqual(obj2.plusOne(), 22);
assert.strictEqual(obj2.plusOne(), 23);
