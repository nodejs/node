'use strict';

// This tests error recovery and fallback behavior for tls.setDefaultCACertificates()

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const { assertEqualCerts } = require('../common/tls');

const fixtureCert = fixtures.readKey('fake-startcom-root-cert.pem');

// Test recovery from errors when setting default CA certificates.
function testRecovery(expectedCerts) {
  {
    const invalidCert = 'not a valid certificate';
    assert.throws(() => tls.setDefaultCACertificates([invalidCert]), {
      code: 'ERR_CRYPTO_OPERATION_FAILED',
      message: /No valid certificates found in the provided array/
    });
    assertEqualCerts(tls.getCACertificates('default'), expectedCerts);
  }

  // Test with mixed valid and invalid certificate formats.
  {
    const invalidCert = '-----BEGIN CERTIFICATE-----\nvalid cert content\n-----END CERTIFICATE-----';
    assert.throws(() => tls.setDefaultCACertificates([fixtureCert, invalidCert]), {
      code: 'ERR_OSSL_PEM_ASN1_LIB',
    });
    assertEqualCerts(tls.getCACertificates('default'), expectedCerts);
  }
}

const originalDefaultCerts = tls.getCACertificates('default');
testRecovery(originalDefaultCerts);

// Check that recovery still works after replacing the default certificates.
const subset = tls.getCACertificates('bundled').slice(0, 3);
tls.setDefaultCACertificates(subset);
assertEqualCerts(tls.getCACertificates('default'), subset);
testRecovery(subset);
