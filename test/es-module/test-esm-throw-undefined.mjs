// Flags: --experimental-modules
/* eslint-disable node-core/required-modules */
import common from '../common/index.js';
import assert from 'assert';

async function doTest() {
  try {
    await import('../fixtures/es-module-loaders/throw-undefined');
    assert(false);
  } catch (e) {
    assert.strictEqual(e, undefined);
  }
}

doTest().then(common.mustCall());
