'use strict';
const common = require('../../common');
const path = require('path');
const assert = require('assert');

// This is a subtest of symlinked-module/test.js. This is not
// intended to be run directly.

module.exports.test = common.mustCall(function test(bindingDir) {
  const mod = require(path.join(bindingDir, 'binding.node'));
  assert.notStrictEqual(mod, null);
  assert.strictEqual(mod.hello(), 'world');
}, require.main === module ? 0 : 2);
