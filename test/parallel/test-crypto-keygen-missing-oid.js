'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
  generateKeyPairSync,
  getCurves,
} = require('crypto');

const { hasOpenSSL, hasFIPS } = require('../common/crypto');

// This test creates EC key pairs on curves without associated OIDs.
// Specifying a key encoding should not crash.
{
  if (process.versions.openssl >= '1.1.1i') {
    for (const namedCurve of ['Oakley-EC2N-3', 'Oakley-EC2N-4']) {
      if (!getCurves().includes(namedCurve))
        continue;

      const expectedError = hasFIPS(3) ? {
        name: 'Error',
        message: 'error:08000081:elliptic curve routines::unknown group',
      } : {
        code: hasOpenSSL(3) ? 'ERR_OSSL_MISSING_OID' : 'ERR_OSSL_EC_MISSING_OID',
      };
      const params = {
        namedCurve,
        publicKeyEncoding: {
          format: 'der',
          type: 'spki'
        }
      };

      assert.throws(() => {
        generateKeyPairSync('ec', params);
      }, expectedError);

      generateKeyPair('ec', params, common.expectsError(expectedError));
    }
  }
}
