'use strict';
// Flags: --expose-gc
const common = require('../../common');
const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);

binding.run();
global.gc();
setImmediate(common.mustCall(() => {
  assert.strictEqual(binding.isAlive(), 0);
}));
