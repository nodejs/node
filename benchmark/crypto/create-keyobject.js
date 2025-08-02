'use strict';

const common = require('../common.js');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const fixtures_keydir = path.resolve(__dirname, '../../test/fixtures/keys/');

function readKey(name) {
  return fs.readFileSync(`${fixtures_keydir}/${name}.pem`, 'utf8');
}

function readKeyPair(publicKeyName, privateKeyName) {
  return {
    publicKey: readKey(publicKeyName),
    privateKey: readKey(privateKeyName),
  };
}

const keyFixtures = {
  'ec': readKeyPair('ec_p256_public', 'ec_p256_private'),
  'rsa': readKeyPair('rsa_public_2048', 'rsa_private_2048'),
  'ed25519': readKeyPair('ed25519_public', 'ed25519_private'),
  'ml-dsa-44': readKeyPair('ml_dsa_44_public', 'ml_dsa_44_private'),
};

const bench = common.createBenchmark(main, {
  keyType: ['rsa', 'ec', 'ed25519', 'ml-dsa-44'],
  keyFormat: ['pkcs8', 'spki', 'der-pkcs8', 'der-spki', 'jwk-public', 'jwk-private'],
  n: [1e3],
});

function measure(n, fn, input) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    fn(input);
  }
  bench.end(n);
}

function main({ n, keyFormat, keyType }) {
  const keyPair = {
    publicKey: crypto.createPublicKey(keyFixtures[keyType].publicKey),
    privateKey: crypto.createPrivateKey(keyFixtures[keyType].privateKey),
  };

  let key, fn;
  switch (keyFormat) {
    case 'spki':
      key = keyPair.publicKey.export({ format: 'pem', type: 'spki' });
      fn = crypto.createPublicKey;
      break;
    case 'pkcs8':
      key = keyPair.privateKey.export({ format: 'pem', type: 'pkcs8' });
      fn = crypto.createPrivateKey;
      break;
    case 'der-spki': {
      const options = { format: 'der', type: 'spki' };
      key = { ...options, key: keyPair.publicKey.export(options) };
      fn = crypto.createPublicKey;
      break;
    }
    case 'der-pkcs8': {
      const options = { format: 'der', type: 'pkcs8' };
      key = { ...options, key: keyPair.privateKey.export(options) };
      fn = crypto.createPrivateKey;
      break;
    }
    case 'jwk-public': {
      const options = { format: 'jwk' };
      key = { ...options, key: keyPair.publicKey.export(options) };
      fn = crypto.createPublicKey;
      break;
    }
    case 'jwk-private': {
      const options = { format: 'jwk' };
      key = { ...options, key: keyPair.privateKey.export(options) };
      fn = crypto.createPrivateKey;
      break;
    }
    default:
      throw new Error('not implemented');
  }

  measure(n, fn, key);
}
