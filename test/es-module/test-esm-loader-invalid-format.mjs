// Flags: --experimental-loader ./test/fixtures/es-module-loaders/loader-invalid-format.mjs
import { expectsError, mustCall } from '../common/index.mjs';
import assert from 'assert';

import('../fixtures/es-modules/test-esm-ok.mjs')
.then(assert.fail, expectsError({
  code: 'ERR_UNKNOWN_FILE_EXTENSION',
  message: /Unknown file extension "" for \/asdf imported from /
}))
.then(mustCall());
