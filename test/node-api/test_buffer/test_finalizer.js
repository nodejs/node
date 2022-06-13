'use strict';
// Flags: --expose-gc --force-node-api-uncaught-exceptions-policy

const common = require('../../common');
const binding = require(`./build/${common.buildType}/test_buffer`);
const assert = require('assert');
const tick = require('util').promisify(require('../../common/tick'));

process.on('uncaughtException', common.mustCall((err) => {
  assert.throws(() => { throw err; }, /finalizer error/);
}));

(async function() {
  {
    binding.malignFinalizerBuffer(common.mustCall(() => {
      throw new Error('finalizer error');
    }));
  }
  global.gc();
  await tick(10);
})().then(common.mustCall());
