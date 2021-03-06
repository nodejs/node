'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const invalidEngineName = 'xxx';

assert.throws(
  () => crypto.setEngine(true),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "id" argument must be of type string. Received type boolean' +
             ' (true)'
  });

assert.throws(
  () => crypto.setEngine('/path/to/engine', 'notANumber'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "flags" argument must be of type number. Received type' +
             " string ('notANumber')"
  });

assert.throws(
  () => crypto.setEngine(invalidEngineName),
  {
    code: 'ERR_CRYPTO_ENGINE_UNKNOWN',
    name: 'Error',
    message: `Engine "${invalidEngineName}" was not found`
  });

assert.throws(
  () => crypto.setEngine(invalidEngineName, crypto.constants.ENGINE_METHOD_RSA),
  {
    code: 'ERR_CRYPTO_ENGINE_UNKNOWN',
    name: 'Error',
    message: `Engine "${invalidEngineName}" was not found`
  });
