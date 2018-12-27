import { expectsError, mustCall, requireFlags } from '../common';
import assert from 'assert';

const flag =
  '--loader=./test/fixtures/es-module-loaders/loader-invalid-url.mjs';
if (!process.execArgv.includes(flag)) {
  // Include `--experimental-modules` explicitly for workers.
  requireFlags(['--experimental-modules', flag]);
}

import('../fixtures/es-modules/test-esm-ok.mjs')
.then(assert.fail, expectsError({
  code: 'ERR_INVALID_RETURN_PROPERTY',
  message: 'Expected a valid url to be returned for the "url" from the ' +
           '"loader resolve" function but got ' +
           '../fixtures/es-modules/test-esm-ok.mjs.'
}))
.then(mustCall());
