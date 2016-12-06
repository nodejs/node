'use strict';
const common = require('../../common');
var assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);
assert.strictEqual(binding.hello(), 'world');
console.log('binding.hello() =', binding.hello());
