'use strict';
const common = require('../../common');
const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);
assert.strictEqual(binding(), 'world');
console.log('binding.hello() =', binding());
