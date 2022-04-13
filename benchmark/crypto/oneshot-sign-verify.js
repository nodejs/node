'use strict';

const common = require('../common.js');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const fixtures_keydir = path.resolve(__dirname, '../../test/fixtures/keys/');
const keyFixtures = {
  publicKey: fs.readFileSync(`${fixtures_keydir}/ec_p256_public.pem`)
    .toString(),
  privateKey: fs.readFileSync(`${fixtures_keydir}/ec_p256_private.pem`)
    .toString()
};

const data = crypto.randomBytes(256);

let pems;
let keyObjects;

function getKeyObject({ privateKey, publicKey }) {
  return {
    privateKey: crypto.createPrivateKey(privateKey),
    publicKey: crypto.createPublicKey(publicKey)
  };
}

const bench = common.createBenchmark(main, {
  mode: ['sync', 'async-serial', 'async-parallel'],
  keyFormat: ['pem', 'keyObject', 'pem.unique', 'keyObject.unique'],
  n: [1e3],
});

function measureSync(n, privateKey, publicKey, keys) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    crypto.verify(
      'sha256',
      data,
      { key: publicKey || keys[i].publicKey, dsaEncoding: 'ieee-p1363' },
      crypto.sign(
        'sha256',
        data,
        { key: privateKey || keys[i].privateKey, dsaEncoding: 'ieee-p1363' }));
  }
  bench.end(n);
}

function measureAsyncSerial(n, privateKey, publicKey, keys) {
  let remaining = n;
  function done() {
    if (--remaining === 0)
      bench.end(n);
    else
      one();
  }

  function one() {
    crypto.sign(
      'sha256',
      data,
      {
        key: privateKey || keys[n - remaining].privateKey,
        dsaEncoding: 'ieee-p1363'
      },
      (err, signature) => {
        crypto.verify(
          'sha256',
          data,
          {
            key: publicKey || keys[n - remaining].publicKey,
            dsaEncoding: 'ieee-p1363'
          },
          signature,
          done);
      });
  }
  bench.start();
  one();
}

function measureAsyncParallel(n, privateKey, publicKey, keys) {
  let remaining = n;
  function done() {
    if (--remaining === 0)
      bench.end(n);
  }
  bench.start();
  for (let i = 0; i < n; ++i) {
    crypto.sign(
      'sha256',
      data,
      { key: privateKey || keys[i].privateKey, dsaEncoding: 'ieee-p1363' },
      (err, signature) => {
        crypto.verify(
          'sha256',
          data,
          { key: publicKey || keys[i].publicKey, dsaEncoding: 'ieee-p1363' },
          signature,
          done);
      });
  }
}

function main({ n, mode, keyFormat }) {
  pems ||= [...Buffer.alloc(n)].map(() => ({
    privateKey: keyFixtures.privateKey,
    publicKey: keyFixtures.publicKey
  }));
  keyObjects ||= pems.map(getKeyObject);

  let privateKey, publicKey, keys;

  switch (keyFormat) {
    case 'keyObject':
      ({ publicKey, privateKey } = keyObjects[0]);
      break;
    case 'pem':
      ({ publicKey, privateKey } = pems[0]);
      break;
    case 'pem.unique':
      keys = pems;
      break;
    case 'keyObject.unique':
      keys = keyObjects;
      break;
    default:
      throw new Error('not implemented');
  }

  switch (mode) {
    case 'sync':
      measureSync(n, privateKey, publicKey, keys);
      break;
    case 'async-serial':
      measureAsyncSerial(n, privateKey, publicKey, keys);
      break;
    case 'async-parallel':
      measureAsyncParallel(n, privateKey, publicKey, keys);
      break;
  }
}
