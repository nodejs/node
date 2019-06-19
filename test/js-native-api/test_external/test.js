'use strict';
// Flags: --expose-gc

const common = require('../../common');
const assert = require('assert');
const util = require('util');

const isExternal = util.types.isExternal;
const test_external = require(`./build/${common.buildType}/test_external`);

const external = test_external.createExternal(10);
const number = test_external.getVal(external);
assert.ok(isExternal(external));
assert.strictEqual(number, 10);
