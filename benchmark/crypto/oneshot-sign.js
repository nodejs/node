'use strict';

const common = require('../common.js');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const fixtures_keydir = path.resolve(__dirname, '../../test/fixtures/keys/');

const keyFixtures = {
  ec: fs.readFileSync(`${fixtures_keydir}/ec_p256_private.pem`, 'utf-8'),
  rsa: fs.readFileSync(`${fixtures_keydir}/rsa_private_2048.pem`, 'utf-8'),
  ed25519: fs.readFileSync(`${fixtures_keydir}/ed25519_private.pem`, 'utf-8'),
};

const data = crypto.randomBytes(256);

let pems;
let keyObjects;

const bench = common.createBenchmark(main, {
  keyType: ['rsa', 'ec', 'ed25519'],
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

function measureSync(n, digest, privateKey, keys) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    crypto.sign(
      digest,
      data,
      privateKey || keys[i]);
  }
  bench.end(n);
}

function measureAsync(n, digest, privateKey, keys) {
  let remaining = n;
  function done() {
    if (--remaining === 0)
      bench.end(n);
    else
      one();
  }

  function one() {
    crypto.sign(
      digest,
      data,
      privateKey || keys[n - remaining],
      done);
  }
  bench.start();
  one();
}

function measureAsyncParallel(n, digest, privateKey, keys) {
  let remaining = n;
  function done() {
    if (--remaining === 0)
      bench.end(n);
  }
  bench.start();
  for (let i = 0; i < n; ++i) {
    crypto.sign(
      digest,
      data,
      privateKey || keys[i],
      done);
  }
}

function main({ n, mode, keyFormat, keyType }) {
  pems ||= [...Buffer.alloc(n)].map(() => keyFixtures[keyType]);
  keyObjects ||= pems.map(crypto.createPrivateKey);

  let privateKey, keys, digest;

  switch (keyType) {
    case 'rsa':
    case 'ec':
      digest = 'sha256';
      break;
    case 'ed25519':
      break;
    default:
      throw new Error('not implemented');
  }

  switch (keyFormat) {
    case 'keyObject':
      privateKey = keyObjects[0];
      break;
    case 'pem':
      privateKey = pems[0];
      break;
    case 'jwk': {
      privateKey = { key: keyObjects[0].export({ format: 'jwk' }), format: 'jwk' };
      break;
    }
    case 'der': {
      privateKey = { key: keyObjects[0].export({ format: 'der', type: 'pkcs8' }), format: 'der', type: 'pkcs8' };
      break;
    }
    case 'keyObject.unique':
      keys = keyObjects;
      break;
    default:
      throw new Error('not implemented');
  }

  switch (mode) {
    case 'sync':
      measureSync(n, digest, privateKey, keys);
      break;
    case 'async':
      measureAsync(n, digest, privateKey, keys);
      break;
    case 'async-parallel':
      measureAsyncParallel(n, digest, privateKey, keys);
      break;
  }
}
