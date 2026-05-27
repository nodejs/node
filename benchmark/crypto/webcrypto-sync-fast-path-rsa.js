'use strict';

const common = require('../common.js');
const {
  fixtureCryptoKeyPair,
  kThresholdSizeLabels,
  measureAsync,
  ptn,
  thresholdSize,
} = require('./_webcrypto_sync_fast_path_common.js');

const { subtle } = globalThis.crypto;

const rsaKeySizes = [
  'fixture-2048',
  'fixture-4096',
];

const bench = common.createBenchmark(main, {
  operation: [
    'rsa-oaep-encrypt',
    'rsa-oaep-decrypt',
    'rsassa-pkcs1-v1_5-verify',
    'rsa-pss-verify',
  ],
  keySize: rsaKeySizes,
  size: kThresholdSizeLabels,
  mode: ['serial', 'parallel'],
  n: [500],
}, {
  combinationFilter({ operation, keySize, size }) {
    if (operation === 'rsa-oaep-encrypt' || operation === 'rsa-oaep-decrypt')
      return size === 'minimal';
    return true;
  },
});

function rsaAlgorithm(name) {
  return {
    name,
    hash: 'SHA-256',
  };
}

function fixtureNames(keySize) {
  const bits = keySize.slice('fixture-'.length);
  return {
    publicKeyName: `rsa_public_${bits}`,
    privateKeyName: `rsa_private_${bits}`,
  };
}

async function rsaKeyPair(operation, keySize) {
  const { publicKeyName, privateKeyName } = fixtureNames(keySize);
  switch (operation) {
    case 'rsa-oaep-encrypt':
    case 'rsa-oaep-decrypt':
      return fixtureCryptoKeyPair(
        publicKeyName,
        privateKeyName,
        rsaAlgorithm('RSA-OAEP'),
        ['encrypt'],
        ['decrypt']);
    case 'rsassa-pkcs1-v1_5-verify':
      return fixtureCryptoKeyPair(
        publicKeyName,
        privateKeyName,
        rsaAlgorithm('RSASSA-PKCS1-v1_5'),
        ['verify'],
        ['sign']);
    case 'rsa-pss-verify':
      return fixtureCryptoKeyPair(
        publicKeyName,
        privateKeyName,
        rsaAlgorithm('RSA-PSS'),
        ['verify'],
        ['sign']);
  }
  throw new Error(`Unknown RSA operation: ${operation}`);
}

async function setupRsaOperation(operation, keySize, size) {
  const pair = await rsaKeyPair(operation, keySize);

  switch (operation) {
    case 'rsa-oaep-encrypt': {
      const data = ptn(32);
      return () => subtle.encrypt('RSA-OAEP', pair.publicKey, data);
    }
    case 'rsa-oaep-decrypt': {
      const data = ptn(32);
      const ciphertext = await subtle.encrypt('RSA-OAEP', pair.publicKey, data);
      return () => subtle.decrypt('RSA-OAEP', pair.privateKey, ciphertext);
    }
    case 'rsassa-pkcs1-v1_5-verify':
    case 'rsa-pss-verify': {
      const data = ptn(thresholdSize(size));
      const signAlgorithm = operation === 'rsa-pss-verify' ?
        { name: 'RSA-PSS', saltLength: 32 } :
        { name: 'RSASSA-PKCS1-v1_5' };
      const signature = await subtle.sign(signAlgorithm, pair.privateKey, data);
      return () => subtle.verify(signAlgorithm, pair.publicKey, signature, data);
    }
  }
  throw new Error(`Unknown RSA operation: ${operation}`);
}

async function main({ n, operation, keySize, size, mode }) {
  const run = await setupRsaOperation(operation, keySize, size);
  await measureAsync(bench, n, mode, run);
}
