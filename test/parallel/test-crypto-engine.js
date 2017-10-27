// Flags: --expose-internals
'use strict';
const common = require('../common');
const { CryptoError } = require('internal/errors');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

common.expectsError(
  () => crypto.setEngine(true),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "id" argument must be of type string'
  });

common.expectsError(
  () => crypto.setEngine('/path/to/engine', 'notANumber'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "flags" argument must be of type number'
  });

common.expectsError(
  () => crypto.setEngine('not a known engine', 1),
  {
    code: 'ERR_CRYPTO_ENGINE_UNKNOWN',
    type: CryptoError,
    message: 'Engine "not a known engine" was not found',
    additional(err) {
      assert(err.info, 'does not have info property');
      assert(Array.isArray(err.opensslErrorStack),
             'opensslErrorStack must be an array');
      assert(typeof err.info.code, 'number');
      assert(typeof err.info.message, 'string');
    }
  }
);
