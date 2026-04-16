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

const keyFixtures = {};

if (hasOpenSSL(3, 5)) {
  keyFixtures['ml-kem-512'] = readKeyPair('ml_kem_512_public', 'ml_kem_512_private');
  keyFixtures['ml-kem-768'] = readKeyPair('ml_kem_768_public', 'ml_kem_768_private');
  keyFixtures['ml-kem-1024'] = readKeyPair('ml_kem_1024_public', 'ml_kem_1024_private');
}
if (hasOpenSSL(3, 2)) {
  keyFixtures['p-256'] = readKeyPair('ec_p256_public', 'ec_p256_private');
  keyFixtures['p-384'] = readKeyPair('ec_p384_public', 'ec_p384_private');
  keyFixtures['p-521'] = readKeyPair('ec_p521_public', 'ec_p521_private');
  keyFixtures.x25519 = readKeyPair('x25519_public', 'x25519_private');
  keyFixtures.x448 = readKeyPair('x448_public', 'x448_private');
}
if (hasOpenSSL(3, 0)) {
  keyFixtures.rsa = readKeyPair('rsa_public_2048', 'rsa_private_2048');
}

if (Object.keys(keyFixtures).length === 0) {
  console.log('no supported key types available for this OpenSSL version');
  process.exit(0);
}

const bench = common.createBenchmark(main, {
  keyType: Object.keys(keyFixtures),
  mode: ['sync', 'async', 'async-parallel'],
  keyFormat: ['keyObject', 'keyObject.unique', 'pem', 'der', 'jwk',
              'raw-public', 'raw-private', 'raw-seed'],
  op: ['encapsulate', 'decapsulate'],
  n: [1e3],
}, {
  combinationFilter(p) {
    // "keyObject.unique" allows to compare the result with "keyObject" to
    // assess whether mutexes over the key material impact the operation
    if (p.keyFormat === 'keyObject.unique')
      return p.mode === 'async-parallel';
    // JWK is not supported for ml-kem for now
    if (p.keyFormat === 'jwk')
      return !p.keyType.startsWith('ml-');
    // raw-public is only supported for encapsulate, not rsa
    if (p.keyFormat === 'raw-public')
      return p.keyType !== 'rsa' && p.op === 'encapsulate';
    // raw-private is not supported for rsa and ml-kem, only for decapsulate
    if (p.keyFormat === 'raw-private')
      return p.keyType !== 'rsa' && !p.keyType.startsWith('ml-') && p.op === 'decapsulate';
    // raw-seed is only supported for ml-kem
    if (p.keyFormat === 'raw-seed')
      return p.keyType.startsWith('ml-');
    return true;
  },
});

function measureSync(n, op, key, keys, ciphertexts) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    const k = key || keys[i];
    if (op === 'encapsulate') {
      crypto.encapsulate(k);
    } else {
      crypto.decapsulate(k, ciphertexts[i]);
    }
  }
  bench.end(n);
}

function measureAsync(n, op, key, keys, ciphertexts) {
  let remaining = n;
  function done() {
    if (--remaining === 0)
      bench.end(n);
    else
      one();
  }

  function one() {
    const k = key || keys[n - remaining];
    if (op === 'encapsulate') {
      crypto.encapsulate(k, done);
    } else {
      crypto.decapsulate(k, ciphertexts[n - remaining], done);
    }
  }
  bench.start();
  one();
}

function measureAsyncParallel(n, op, key, keys, ciphertexts) {
  let remaining = n;
  function done() {
    if (--remaining === 0)
      bench.end(n);
  }
  bench.start();
  for (let i = 0; i < n; ++i) {
    const k = key || keys[i];
    if (op === 'encapsulate') {
      crypto.encapsulate(k, done);
    } else {
      crypto.decapsulate(k, ciphertexts[i], done);
    }
  }
}

function main({ n, mode, keyFormat, keyType, op }) {
  const isEncapsulate = op === 'encapsulate';
  const pemSource = isEncapsulate ?
    keyFixtures[keyType].publicKey :
    keyFixtures[keyType].privateKey;
  const createKeyFn = isEncapsulate ? crypto.createPublicKey : crypto.createPrivateKey;
  const pems = [...Buffer.alloc(n)].map(() => pemSource);
  const keyObjects = pems.map(createKeyFn);

  // Warm up OpenSSL's provider operation cache for each key object
  if (isEncapsulate) {
    for (const keyObject of keyObjects) {
      crypto.encapsulate(keyObject);
    }
  } else {
    const warmupCiphertext = crypto.encapsulate(keyObjects[0]).ciphertext;
    for (const keyObject of keyObjects) {
      crypto.decapsulate(keyObject, warmupCiphertext);
    }
  }

  const asymmetricKeyType = keyObjects[0].asymmetricKeyType;
  let key, keys, ciphertexts;

  switch (keyFormat) {
    case 'keyObject':
      key = keyObjects[0];
      break;
    case 'pem':
      key = pems[0];
      break;
    case 'jwk': {
      key = { key: keyObjects[0].export({ format: 'jwk' }), format: 'jwk' };
      break;
    }
    case 'der': {
      const type = isEncapsulate ? 'spki' : 'pkcs8';
      key = { key: keyObjects[0].export({ format: 'der', type }), format: 'der', type };
      break;
    }
    case 'raw-public': {
      const exportedKey = keyObjects[0].export({ format: 'raw-public' });
      const keyOpts = { key: exportedKey, format: 'raw-public', asymmetricKeyType };
      if (asymmetricKeyType === 'ec') keyOpts.namedCurve = keyObjects[0].asymmetricKeyDetails.namedCurve;
      key = keyOpts;
      break;
    }
    case 'raw-private': {
      const exportedKey = keyObjects[0].export({ format: 'raw-private' });
      const keyOpts = { key: exportedKey, format: 'raw-private', asymmetricKeyType };
      if (asymmetricKeyType === 'ec') keyOpts.namedCurve = keyObjects[0].asymmetricKeyDetails.namedCurve;
      key = keyOpts;
      break;
    }
    case 'raw-seed': {
      // raw-seed requires a private key to export from
      const privateKeyObject = crypto.createPrivateKey(keyFixtures[keyType].privateKey);
      key = {
        key: privateKeyObject.export({ format: 'raw-seed' }),
        format: 'raw-seed',
        asymmetricKeyType,
      };
      break;
    }
    case 'keyObject.unique':
      keys = keyObjects;
      break;
    default:
      throw new Error('not implemented');
  }

  // Pre-generate ciphertexts for decapsulate operations
  if (!isEncapsulate) {
    const encapKey = crypto.createPublicKey(
      crypto.createPrivateKey(keyFixtures[keyType].privateKey));
    if (key) {
      ciphertexts = [...Buffer.alloc(n)].map(() => crypto.encapsulate(encapKey).ciphertext);
    } else {
      ciphertexts = keys.map(() => crypto.encapsulate(encapKey).ciphertext);
    }
  }

  switch (mode) {
    case 'sync':
      measureSync(n, op, key, keys, ciphertexts);
      break;
    case 'async':
      measureAsync(n, op, key, keys, ciphertexts);
      break;
    case 'async-parallel':
      measureAsyncParallel(n, op, key, keys, ciphertexts);
      break;
  }
}
