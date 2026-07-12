'use strict';

// This tests input validation of tls.setDefaultCACertificates().

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const assert = require('assert');
const tls = require('tls');
const { assertEqualCerts } = require('../common/tls');

const defaultCerts = tls.getCACertificates('default');
const fixtureCert = fixtures.readKey('fake-startcom-root-cert.pem');

for (const invalid of [null, undefined, 'string', 42, {}, true]) {
  // Test input validation - should throw when not passed an array
  assert.throws(() => tls.setDefaultCACertificates(invalid), {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /The "certs" argument must be an instance of Array/
  });
  // Verify that default certificates remain unchanged after error.
  assertEqualCerts(tls.getCACertificates('default'), defaultCerts);
}

for (const invalid of [null, undefined, 42, {}, true]) {
  // Test input validation - should throw when passed an array with invalid elements
  assert.throws(() => tls.setDefaultCACertificates([invalid]), {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /The "certs\[0\]" argument must be of type string or an instance of ArrayBufferView/
  });
  // Verify that default certificates remain unchanged after error.
  assertEqualCerts(tls.getCACertificates('default'), defaultCerts);

  assert.throws(() => tls.setDefaultCACertificates([fixtureCert, invalid]), {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /The "certs\[1\]" argument must be of type string or an instance of ArrayBufferView/
  });
  // Verify that default certificates remain unchanged after error.
  assertEqualCerts(tls.getCACertificates('default'), defaultCerts);
}
