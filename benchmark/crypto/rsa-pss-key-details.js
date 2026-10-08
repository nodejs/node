'use strict';

const common = require('../common.js');
const { isBoringSSL } = require('../../test/common/crypto.js');
const { createPublicKey, generateKeyPairSync } = require('crypto');

if (isBoringSSL) {
  console.log('Skipping: RSA-PSS key generation is not supported by BoringSSL');
  process.exit(0);
}

const restrictions = {
  absent: {},
  defaults: { hashAlgorithm: 'sha1', mgf1HashAlgorithm: 'sha1', saltLength: 20 },
  sha256: { hashAlgorithm: 'sha256', mgf1HashAlgorithm: 'sha256', saltLength: 32 },
};

const bench = common.createBenchmark(main, {
  restrictions: Object.keys(restrictions),
  n: [5000],
});

function main({ restrictions: name, n }) {
  const { privateKey } = generateKeyPairSync('rsa-pss', {
    modulusLength: 2048,
    ...restrictions[name],
  });
  // Use a fresh KeyObject without decoding DER or reusing cached key details.
  bench.start();
  for (let index = 0; index < n; index++) {
    if (createPublicKey(privateKey).asymmetricKeyDetails.modulusLength !== 2048)
      throw new Error('Unexpected modulus length');
  }
  bench.end(n);
}
