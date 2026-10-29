// This tests that after failing to require an ESM that throws,
// retrying with import() still rejects.

'use strict';
const common = require('../common');
const assert = require('assert');

assert.throws(() => {
  require('../fixtures/es-modules/throw-error.mjs');
}, {
  message: 'test',
});

assert.rejects(import('../fixtures/es-modules/throw-error.mjs'), {
  message: 'test',
}).then(common.mustCall());
