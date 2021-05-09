// Flags: --experimental-loader ./test/fixtures/es-module-loaders/loader-invalid-url.mjs
import { expectsError, mustCall } from '../common/index.mjs';
import assert from 'assert';

import('../fixtures/es-modules/test-esm-ok.mjs')
.then(assert.fail, (error) => {
  expectsError({
    code: 'ERR_INVALID_URL',
    message: 'Invalid URL'
  })(error);

  assert.strictEqual(error.input, '../fixtures/es-modules/test-esm-ok.mjs');
})
.then(mustCall());
