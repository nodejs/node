'use strict';
const common = require('../../common');
const assert = require('assert');
const binding = require(require.resolve(`./build/${common.buildType}/binding`));
assert.strictEqual(binding.hello(), 'world');
