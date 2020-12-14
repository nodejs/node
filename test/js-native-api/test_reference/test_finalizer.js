'use strict';
// Flags: --expose-gc --force-node-api-uncaught-exceptions-policy

const common = require('../../common');
const test_reference = require(`./build/${common.buildType}/test_reference`);
const assert = require('assert');

process.on('uncaughtException', common.mustCall((err) => {
  assert.throws(() => { throw err; }, /finalizer error/);
}));

(async function() {
  {
    test_reference.createExternalWithJsFinalize(
      common.mustCall(() => {
        throw new Error('finalizer error');
      }));
  }
  global.gc();
})().then(common.mustCall());
