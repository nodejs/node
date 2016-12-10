'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
if (common.hasFipsCrypto) {
  common.skip('BF-ECB is not FIPS 140-2 compatible');
  return;
}
const crypto = require('crypto');

crypto.DEFAULT_ENCODING = 'buffer';

// Testing whether EVP_CipherInit_ex is functioning correctly.
// Reference: bug#1997

{
  const encrypt =
    crypto.createCipheriv('BF-ECB', 'SomeRandomBlahz0c5GZVnR', '');
  let hex = encrypt.update('Hello World!', 'ascii', 'hex');
  hex += encrypt.final('hex');
  assert.strictEqual(hex.toUpperCase(), '6D385F424AAB0CFBF0BB86E07FFB7D71');
}

{
  const decrypt =
    crypto.createDecipheriv('BF-ECB', 'SomeRandomBlahz0c5GZVnR', '');
  let msg = decrypt.update('6D385F424AAB0CFBF0BB86E07FFB7D71', 'hex', 'ascii');
  msg += decrypt.final('ascii');
  assert.strictEqual(msg, 'Hello World!');
}
