'use strict';

const common = require('../common.js');
const { hasOpenSSL, hasFIPS, isBoringSSL } = require('../../test/common/crypto.js');
const fixtures = require('../../test/common/fixtures.js');
const { createPrivateKey, createPublicKey } = require('crypto');

const keys = {
  'rsa': 'rsa_private_2048',
  'rsa-pss': 'rsa_pss_private_2048',
  'p-256': 'ec_p256_private',
  'p-384': 'ec_p384_private',
  'p-521': 'ec_p521_private',
  'ed25519': 'ed25519_private',
  'x25519': 'x25519_private',
};
if (!isBoringSSL) {
  keys.ed448 = 'ed448_private';
  keys.x448 = 'x448_private';
  if (!hasFIPS()) keys['rsa-multiprime'] = 'rsa_private_2048_3_primes';
}
if (hasOpenSSL(3, 5) || isBoringSSL) {
  keys['ml-dsa-44'] = 'ml_dsa_44_private_seed_only';
  keys['ml-dsa-87'] = 'ml_dsa_87_private_seed_only';
  keys['ml-kem-768'] = 'ml_kem_768_private_seed_only';
  keys['ml-kem-1024'] = 'ml_kem_1024_private_seed_only';
}
if (hasOpenSSL(3, 5)) {
  keys['slh-dsa-sha2-128s'] = 'slh_dsa_sha2_128s_private';
  keys['slh-dsa-shake-256s'] = 'slh_dsa_shake_256s_private';
}

const bench = common.createBenchmark(main, {
  // Keep one size per family by default; other fixtures remain available via keyType.
  keyType: ['rsa', 'rsa-pss', 'p-256', 'ed25519', 'x25519',
            'ml-dsa-44', 'ml-kem-768', 'slh-dsa-sha2-128s'].filter((type) => keys[type]),
  operation: ['import', 'export'],
  // PEM can be selected with format=pem; DER covers ASN.1 encoding by default.
  format: ['jwk', 'der', 'raw-public', 'raw-private', 'raw-seed'],
  type: ['public', 'private'],
  n: [1e4],
}, {
  combinationFilter({ keyType, format, type }) {
    if (format === 'jwk') return keyType !== 'rsa-pss';
    if (!format.startsWith('raw-')) return true;
    if (keyType.startsWith('rsa')) return false;
    if (format === 'raw-public') return type === 'public';
    if (format === 'raw-private') return type === 'private' && !keyType.startsWith('ml-');
    return type === 'private' && keyType.startsWith('ml-');
  },
});

function main({ keyType, operation, format, type, n }) {
  const privateKey = createPrivateKey(fixtures.readKey(`${keys[keyType]}.pem`));
  const key = type === 'private' ? privateKey : createPublicKey(privateKey);
  const options = { format };
  if (format === 'pem' || format === 'der') {
    options.type = type === 'private' ? 'pkcs8' : 'spki';
  }
  let run;
  if (operation === 'export') {
    run = () => key.export(options);
  } else {
    const input = { ...options, key: key.export(options) };
    if (format.startsWith('raw-')) {
      input.asymmetricKeyType = key.asymmetricKeyType;
      if (input.asymmetricKeyType === 'ec') {
        input.namedCurve = key.asymmetricKeyDetails.namedCurve;
      }
    }
    const importKey = type === 'private' ? createPrivateKey : createPublicKey;
    run = () => importKey(input);
  }
  // Resolve provider operations and warm the JS path before timing.
  for (let i = 0; i < 100; i++) run();
  bench.start();
  for (let i = 0; i < n; i++) run();
  bench.end(n);
}
