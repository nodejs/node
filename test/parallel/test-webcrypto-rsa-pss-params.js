'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (common.hasOpenSSL3)
  common.skip('temporarily skipping for OpenSSL 3.0-alpha15');

const {
  createPrivateKey,
  createPublicKey,
  webcrypto: {
    subtle
  }
} = require('crypto');

const fixtures = require('../common/fixtures');

{
  const rsaPssKeyWithoutParams = fixtures.readKey('rsa_pss_private_2048.pem');

  const pkcs8 = createPrivateKey(rsaPssKeyWithoutParams).export({
    type: 'pkcs8',
    format: 'der'
  });
  const spki = createPublicKey(rsaPssKeyWithoutParams).export({
    type: 'spki',
    format: 'der'
  });

  const hashes = ['SHA-1', 'SHA-256', 'SHA-384', 'SHA-512'];

  const tasks = [];
  for (const hash of hashes) {
    const algorithm = { name: 'RSA-PSS', hash };
    tasks.push(subtle.importKey('pkcs8', pkcs8, algorithm, true, ['sign']));
    tasks.push(subtle.importKey('spki', spki, algorithm, true, ['verify']));
  }

  Promise.all(tasks).then(common.mustCall());
}
