'use strict';

const common = require('../common.js');
const { hasOpenSSL } = require('../../test/common/crypto.js');
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
};

if (hasOpenSSL(3, 5)) {
  keyFixtures['ml-dsa-44'] = readKeyPair('ml_dsa_44_public', 'ml_dsa_44_private');
}

const data = crypto.randomBytes(256);

let pems;
let keyObjects;

const bench = common.createBenchmark(main, {
  keyType: Object.keys(keyFixtures),
  mode: ['sync', 'async', 'async-parallel'],
  keyFormat: ['pem', 'der', 'jwk', 'keyObject', 'keyObject.unique'],
  n: [1e3],
}, {
  combinationFilter(p) {
    // "keyObject.unique" allows to compare the result with "keyObject" to
    // assess whether mutexes over the key material impact the operation
    return p.keyFormat !== 'keyObject.unique' ||
      (p.keyFormat === 'keyObject.unique' && p.mode === 'async-parallel');
  },
});

function measureSync(n, digest, signature, publicKey, keys) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    crypto.verify(
      digest,
      data,
      publicKey || keys[i],
      signature);
  }
  bench.end(n);
}

function measureAsync(n, digest, signature, publicKey, keys) {
  let remaining = n;
  function done() {
    if (--remaining === 0)
      bench.end(n);
    else
      one();
  }

  function one() {
    crypto.verify(
      digest,
      data,
      publicKey || keys[n - remaining],
      signature,
      done);
  }
  bench.start();
  one();
}

function measureAsyncParallel(n, digest, signature, publicKey, keys) {
  let remaining = n;
  function done() {
    if (--remaining === 0)
      bench.end(n);
  }
  bench.start();
  for (let i = 0; i < n; ++i) {
    crypto.verify(
      digest,
      data,
      publicKey || keys[i],
      signature,
      done);
  }
}

function main({ n, mode, keyFormat, keyType }) {
  pems ||= [...Buffer.alloc(n)].map(() => keyFixtures[keyType].publicKey);
  keyObjects ||= pems.map(crypto.createPublicKey);

  let publicKey, keys, digest;

  switch (keyType) {
    case 'rsa':
    case 'ec':
      digest = 'sha256';
      break;
    case 'ed25519':
    case 'ml-dsa-44':
      break;
    default:
      throw new Error('not implemented');
  }

  switch (keyFormat) {
    case 'keyObject':
      publicKey = keyObjects[0];
      break;
    case 'pem':
      publicKey = pems[0];
      break;
    case 'jwk': {
      publicKey = { key: keyObjects[0].export({ format: 'jwk' }), format: 'jwk' };
      break;
    }
    case 'der': {
      publicKey = { key: keyObjects[0].export({ format: 'der', type: 'spki' }), format: 'der', type: 'spki' };
      break;
    }
    case 'keyObject.unique':
      keys = keyObjects;
      break;
    default:
      throw new Error('not implemented');
  }


  const { privateKey } = keyFixtures[keyType];
  const signature = crypto.sign(digest, data, privateKey);

  switch (mode) {
    case 'sync':
      measureSync(n, digest, signature, publicKey, keys);
      break;
    case 'async':
      measureAsync(n, digest, signature, publicKey, keys);
      break;
    case 'async-parallel':
      measureAsyncParallel(n, digest, signature, publicKey, keys);
      break;
    default:
      throw new Error('not implemented');
  }
}
