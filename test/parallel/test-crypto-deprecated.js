'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const crypto = require('crypto');
const tls = require('tls');

const expected = [
  'crypto.Credentials is deprecated. Use tls.SecureContext instead.',
  'crypto.createCredentials is deprecated. Use tls.createSecureContext instead.'
];

process.on('warning', common.mustCall((warning) => {
  assert.strictEqual(warning.name, 'DeprecationWarning');
  assert.notStrictEqual(expected.indexOf(warning.message), -1,
                        `unexpected error message: "${warning.message}"`);
  // Remove a warning message after it is seen so that we guarantee that we get
  // each message only once.
  expected.splice(expected.indexOf(warning.message), 1);
}, expected.length));

// Accessing the deprecated function is enough to trigger the warning event.
// It does not need to be called. So the assert serves the purpose of both
// triggering the warning event and confirming that the deprected function is
// mapped to the correct non-deprecated function.
assert.strictEqual(crypto.Credentials, tls.SecureContext);
assert.strictEqual(crypto.createCredentials, tls.createSecureContext);
