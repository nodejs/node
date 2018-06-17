// Flags: --experimental-modules --loader ./test/fixtures/es-module-loaders/loader-invalid-url.mjs
/* eslint-disable node-core/required-modules */
import assert from 'assert';
import common from '../common';

import('../fixtures/es-modules/test-esm-ok.mjs')
.then(() => {
  assert.fail();
}, (err) => {
  assert.strictEqual(err.code, 'ERR_INVALID_RETURN_PROPERTY_STRING');
})
.then(common.mustCall());
