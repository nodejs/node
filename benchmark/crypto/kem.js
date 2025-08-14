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

const keyFixtures = {};

if (hasOpenSSL(3, 5)) {
  keyFixtures['ml-kem-512'] = readKey('ml_kem_512_private');
  keyFixtures['ml-kem-768'] = readKey('ml_kem_768_private');
  keyFixtures['ml-kem-1024'] = readKey('ml_kem_1024_private');
}
if (hasOpenSSL(3, 2)) {
  keyFixtures['p-256'] = readKey('ec_p256_private');
  keyFixtures['p-384'] = readKey('ec_p384_private');
  keyFixtures['p-521'] = readKey('ec_p521_private');
  keyFixtures.x25519 = readKey('x25519_private');
  keyFixtures.x448 = readKey('x448_private');
}
if (hasOpenSSL(3, 0)) {
  keyFixtures.rsa = readKey('rsa_private_2048');
}

if (Object.keys(keyFixtures).length === 0) {
  console.log('no supported key types available for this OpenSSL version');
  process.exit(0);
}

const bench = common.createBenchmark(main, {
  keyType: Object.keys(keyFixtures),
  mode: ['sync', 'async', 'async-parallel'],
  keyFormat: ['keyObject', 'keyObject.unique'],
  op: ['encapsulate', 'decapsulate'],
  n: [1e3],
}, {
  combinationFilter(p) {
    // "keyObject.unique" allows to compare the result with "keyObject" to
    // assess whether mutexes over the key material impact the operation
    return p.keyFormat !== 'keyObject.unique' ||
      (p.keyFormat === 'keyObject.unique' && p.mode === 'async-parallel');
  },
});

function measureSync(n, op, privateKey, keys, ciphertexts) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    const key = privateKey || keys[i];
    if (op === 'encapsulate') {
      crypto.encapsulate(key);
    } else {
      crypto.decapsulate(key, ciphertexts[i]);
    }
  }
  bench.end(n);
}

function measureAsync(n, op, privateKey, keys, ciphertexts) {
  let remaining = n;
  function done() {
    if (--remaining === 0)
      bench.end(n);
    else
      one();
  }

  function one() {
    const key = privateKey || keys[n - remaining];
    if (op === 'encapsulate') {
      crypto.encapsulate(key, done);
    } else {
      crypto.decapsulate(key, ciphertexts[n - remaining], done);
    }
  }
  bench.start();
  one();
}

function measureAsyncParallel(n, op, privateKey, keys, ciphertexts) {
  let remaining = n;
  function done() {
    if (--remaining === 0)
      bench.end(n);
  }
  bench.start();
  for (let i = 0; i < n; ++i) {
    const key = privateKey || keys[i];
    if (op === 'encapsulate') {
      crypto.encapsulate(key, done);
    } else {
      crypto.decapsulate(key, ciphertexts[i], done);
    }
  }
}

function main({ n, mode, keyFormat, keyType, op }) {
  const pems = [...Buffer.alloc(n)].map(() => keyFixtures[keyType]);
  const keyObjects = pems.map(crypto.createPrivateKey);

  let privateKey, keys, ciphertexts;

  switch (keyFormat) {
    case 'keyObject':
      privateKey = keyObjects[0];
      break;
    case 'keyObject.unique':
      keys = keyObjects;
      break;
    default:
      throw new Error('not implemented');
  }

  // Pre-generate ciphertexts for decapsulate operations
  if (op === 'decapsulate') {
    if (privateKey) {
      ciphertexts = [...Buffer.alloc(n)].map(() => crypto.encapsulate(privateKey).ciphertext);
    } else {
      ciphertexts = keys.map((key) => crypto.encapsulate(key).ciphertext);
    }
  }

  switch (mode) {
    case 'sync':
      measureSync(n, op, privateKey, keys, ciphertexts);
      break;
    case 'async':
      measureAsync(n, op, privateKey, keys, ciphertexts);
      break;
    case 'async-parallel':
      measureAsyncParallel(n, op, privateKey, keys, ciphertexts);
      break;
  }
}
