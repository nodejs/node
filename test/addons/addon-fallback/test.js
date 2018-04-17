'use strict';
const common = require('../../common');
const assert = require('assert');
const bindingPath = require.resolve(`./build/${common.buildType}/binding`);
const binding = require(bindingPath);
assert.strictEqual(binding.answer, 42);

// Test multiple loading of the same module.
delete require.cache[bindingPath];
const rebinding = require(bindingPath);
assert.strictEqual(rebinding.answer, 42);
assert.notStrictEqual(binding, rebinding);
