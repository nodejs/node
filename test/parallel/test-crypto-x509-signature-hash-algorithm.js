'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const assert = require('node:assert/strict');
const { X509Certificate } = require('node:crypto');

// Verify that the signature digest is correctly identified,
// including parameterized signature algorithms such as RSA-PSS.

const testCases = [
  ['rsa-pss-sha1', 'sha1'],
  ['rsa-sha256', 'sha256'],
  ['rsa-pss-sha256', 'sha256'],
  ['rsa-pss-sha384', 'sha384'],
  ['rsa-pss-sha256-mgf1-sha384', 'sha256'],
  ['ed25519', undefined],
];

for (const [name, expected] of testCases) {
  const cert = new X509Certificate(
    fixtures.readKey(`x509-signature-hash/${name}.pem`)
  );

  assert.strictEqual(
    cert.signatureHashAlgorithm,
    expected,
    name
  );

  // Verify that repeated access returns the same value.
  assert.strictEqual(
    cert.signatureHashAlgorithm,
    expected,
    `${name}: cached value`
  );
}
