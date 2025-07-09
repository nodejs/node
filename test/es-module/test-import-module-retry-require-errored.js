// This tests that after failing to import an ESM that rejects,
// retrying with require() still throws.

'use strict';
const common = require('../common');
const assert = require('assert');

(async () => {
  await assert.rejects(import('../fixtures/es-modules/throw-error.mjs'), {
    message: 'test',
  });
  assert.throws(() => {
    require('../fixtures/es-modules/throw-error.mjs');
  }, {
    message: 'test',
  });
})().catch(common.mustNotCall());
