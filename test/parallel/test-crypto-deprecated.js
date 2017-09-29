'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const crypto = require('crypto');
const tls = require('tls');

common.expectWarning('DeprecationWarning', [
  'crypto.Credentials is deprecated. Use tls.SecureContext instead.',
  'crypto.createCredentials is deprecated. ' +
  'Use tls.createSecureContext instead.',
  'crypto.createCipher is deprecated. Use crypto.createCipheriv instead.',
  'crypto.Cipher is deprecated. Use crypto.createCipheriv instead.',
]);

// Accessing the deprecated function is enough to trigger the warning event.
// It does not need to be called. So the assert serves the purpose of both
// triggering the warning event and confirming that the deprected function is
// mapped to the correct non-deprecated function.
assert.strictEqual(crypto.Credentials, tls.SecureContext);
assert.strictEqual(crypto.createCredentials, tls.createSecureContext);

crypto.createCipher;
crypto.Cipher;


if (!common.hasFipsCrypto) {
  assert.doesNotThrow(() => {
    crypto.createCipher('aes-128-ecb', 'this-should-warn');
  });
} else {
  assert.throws(() => {
    crypto.createCipher('aes-128-ecb', 'this-should-throw');
  }, /not supported in FIPS mode/);
}