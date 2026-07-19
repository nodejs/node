'use strict';
// Flags: --expose-gc --force-node-api-uncaught-exceptions-policy

const common = require('../../common');
const binding = require(`./build/${common.buildType}/test_finalizer`);
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
  global.gc({ type: 'minor' });
  await tick(common.platformTimeout(100));
  global.gc();
  await tick(common.platformTimeout(100));
  global.gc();
  await tick(common.platformTimeout(100));
})().then(common.mustCall());
