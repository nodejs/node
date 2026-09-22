'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { generateKeyPair } = require('crypto');
const { inspect } = require('util');
const { hasOpenSSL, isBoringSSL } = require('../common/crypto');

// Test RSA parameters.
{
  // Test invalid modulus lengths. (non-number)
  for (const modulusLength of [undefined, null, 'a', true, {}, []]) {
    assert.throws(() => generateKeyPair('rsa', {
      modulusLength
    }, common.mustNotCall()), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message:
        'The "options.modulusLength" property must be of type number.' +
        common.invalidArgTypeHelper(modulusLength)
    });
  }

  // Test invalid modulus lengths. (non-integer)
  for (const modulusLength of [512.1, 1.3, 1.1, 5000.9, 100.5]) {
    assert.throws(() => generateKeyPair('rsa', {
      modulusLength
    }, common.mustNotCall()), {
      name: 'RangeError',
      code: 'ERR_OUT_OF_RANGE',
      message:
        'The value of "options.modulusLength" is out of range. ' +
        'It must be an integer. ' +
        `Received ${inspect(modulusLength)}`
    });
  }

  // Test invalid modulus lengths. (out of range)
  for (const modulusLength of [-1, -9, 4294967297]) {
    assert.throws(() => generateKeyPair('rsa', {
      modulusLength
    }, common.mustNotCall()), {
      name: 'RangeError',
      code: 'ERR_OUT_OF_RANGE',
    });
  }

  // Test invalid exponents. (non-number)
  for (const publicExponent of ['a', true, {}, []]) {
    assert.throws(() => generateKeyPair('rsa', {
      modulusLength: 4096,
      publicExponent
    }, common.mustNotCall()), {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE',
      message:
        'The "options.publicExponent" property must be of type number.' +
        common.invalidArgTypeHelper(publicExponent)
    });
  }

  // Test invalid exponents. (non-integer)
  for (const publicExponent of [3.5, 1.1, 50.5, 510.5]) {
    assert.throws(() => generateKeyPair('rsa', {
      modulusLength: 4096,
      publicExponent
    }, common.mustNotCall()), {
      name: 'RangeError',
      code: 'ERR_OUT_OF_RANGE',
      message:
        'The value of "options.publicExponent" is out of range. ' +
        'It must be an integer. ' +
        `Received ${inspect(publicExponent)}`
    });
  }

  // Test invalid exponents. (out of range)
  for (const publicExponent of [-5, -3, 4294967297]) {
    assert.throws(() => generateKeyPair('rsa', {
      modulusLength: 4096,
      publicExponent
    }, common.mustNotCall()), {
      name: 'RangeError',
      code: 'ERR_OUT_OF_RANGE',
    });
  }

  // Test invalid exponents. (caught by OpenSSL)
  let invalidExponentError = /bad e value/;
  if (isBoringSSL) {
    invalidExponentError = /BAD_E_VALUE/;
  } else if (hasOpenSSL(3)) {
    invalidExponentError = /exponent/;
  }
  for (const publicExponent of [1, 1 + 0x10001]) {
    generateKeyPair('rsa', {
      modulusLength: 4096,
      publicExponent
    }, common.mustCall((err) => {
      assert.strictEqual(err.name, 'Error');
      assert.match(err.message, invalidExponentError);
    }));
  }
}
