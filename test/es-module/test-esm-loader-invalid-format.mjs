import { expectsError, mustCall, requireFlags } from '../common';
import assert from 'assert';

const flag =
  '--loader=./test/fixtures/es-module-loaders/loader-invalid-format.mjs';

if (!process.execArgv.includes(flag)) {
  // Include `--experimental-modules` explicitly for workers.
  requireFlags('--experimental-modules', flag);
}

import('../fixtures/es-modules/test-esm-ok.mjs')
.then(assert.fail, expectsError({
  code: 'ERR_INVALID_RETURN_PROPERTY_VALUE',
  message: 'Expected string to be returned for the "format" from the ' +
           '"loader resolve" function but got type undefined.'
}))
.then(mustCall());
