// Flags: --experimental-loader ./test/fixtures/es-module-loaders/loader-invalid-url.mjs
import { expectsError, mustCall } from '../common/index.mjs';
import assert from 'assert';

import('../fixtures/es-modules/test-esm-ok.mjs')
.then(assert.fail, (error) => {
  expectsError({ code: 'ERR_INVALID_RETURN_PROPERTY_VALUE' })(error);
  assert.match(error.message, /loader-invalid-url\.mjs/);
})
.then(mustCall());
