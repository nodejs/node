// Flags: --experimental-modules --loader ./test/fixtures/es-module-loaders/loader-invalid-url.mjs
/* eslint-disable node-core/required-modules */

import { expectsError, mustCall } from '../common/index.mjs';
import assert from 'assert';

import('../fixtures/es-modules/test-esm-ok.mjs')
.then(assert.fail, expectsError({
  code: 'ERR_INVALID_RETURN_PROPERTY',
  message: 'Expected a valid url to be returned for the "url" from the ' +
           '"loader resolve" function but got ' +
           '../fixtures/es-modules/test-esm-ok.mjs.'
}))
.then(mustCall());
