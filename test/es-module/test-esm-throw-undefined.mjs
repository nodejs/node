// Flags: --experimental-modules
/* eslint-disable node-core/required-modules */
import common from '../common/index.js';
import assert from 'assert';

async function doTest() {
  await assert.rejects(
    async () => {
      await import('../fixtures/es-module-loaders/throw-undefined');
    },
    (e) => e === undefined
  );
}

doTest().then(common.mustCall());
