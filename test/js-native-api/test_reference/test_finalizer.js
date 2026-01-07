'use strict';
// Flags: --expose-gc --force-node-api-uncaught-exceptions-policy
// Addons: test_finalizer, test_finalizer_vtable

const common = require('../../common');
const { addonPath } = require('../../common/addon-test');
const binding = require(addonPath);
const assert = require('assert');

process.on('uncaughtException', common.mustCall((err) => {
  assert.throws(() => { throw err; }, /finalizer error/);
}));

(async function() {
  {
    binding.createExternalWithJsFinalize(
      common.mustCall(() => {
        throw new Error('finalizer error');
      }));
  }
  global.gc();
})().then(common.mustCall());
