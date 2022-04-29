'use strict';
const common = require('../../common');
const assert = require('assert');
const addon = require(`./build/${common.buildType}/test_proxy_wrap`);

const simpleObj = addon.CreateSimpleObject();
const result1 = simpleObj.add(5, 12);
assert.strictEqual(result1, 17);
