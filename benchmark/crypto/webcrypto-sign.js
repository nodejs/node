'use strict';

const common = require('../common.js');
const { hasOpenSSL } = require('../../test/common/crypto.js');
const { subtle } = globalThis.crypto;

const kAlgorithms = {
  'ec': { name: 'ECDSA', namedCurve: 'P-256' },
  'rsassa-pkcs1-v1_5': {
    name: 'RSASSA-PKCS1-v1_5',
    modulusLength: 2048,
    publicExponent: new Uint8Array([1, 0, 1]),
    hash: 'SHA-256',
  },
  'rsa-pss': {
    name: 'RSA-PSS',
    modulusLength: 2048,
    publicExponent: new Uint8Array([1, 0, 1]),
    hash: 'SHA-256',
  },
  'ed25519': { name: 'Ed25519' },
};

if (hasOpenSSL(3, 5)) {
  kAlgorithms['ml-dsa-44'] = { name: 'ML-DSA-44' };
}

const kSignParams = {
  'ec': { name: 'ECDSA', hash: 'SHA-256' },
  'rsassa-pkcs1-v1_5': { name: 'RSASSA-PKCS1-v1_5' },
  'rsa-pss': { name: 'RSA-PSS', saltLength: 32 },
  'ed25519': { name: 'Ed25519' },
  'ml-dsa-44': { name: 'ML-DSA-44' },
};

const data = globalThis.crypto.getRandomValues(new Uint8Array(256));

let keys;

const bench = common.createBenchmark(main, {
  keyType: Object.keys(kAlgorithms),
  mode: ['serial', 'parallel'],
  keyReuse: ['shared', 'unique'],
  n: [1e3],
}, {
  combinationFilter(p) {
    // Unique only differs from shared when operations overlap (parallel);
    // sequential calls have no contention so unique+serial adds no value.
    if (p.keyReuse === 'unique') return p.mode === 'parallel';
    return true;
  },
});

async function measureSerial(n, signParams, sharedKey) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    await subtle.sign(signParams, sharedKey || keys[i], data);
  }
  bench.end(n);
}

async function measureParallel(n, signParams, sharedKey) {
  const promises = new Array(n);
  bench.start();
  for (let i = 0; i < n; ++i) {
    promises[i] = subtle.sign(signParams, sharedKey || keys[i], data);
  }
  await Promise.all(promises);
  bench.end(n);
}

async function main({ n, mode, keyReuse, keyType }) {
  const algorithm = kAlgorithms[keyType];
  const signParams = kSignParams[keyType];

  if (!keys || keys.length !== n || keys[0].algorithm.name !== signParams.name) {
    keys = new Array(n);
    // Generate one key pair, then import its pkcs8 bytes n times to get
    // distinct CryptoKey instances.
    const kp = await subtle.generateKey(algorithm, true, ['sign', 'verify']);
    const pkcs8 = await subtle.exportKey('pkcs8', kp.privateKey);
    for (let i = 0; i < n; ++i) {
      keys[i] = await subtle.importKey('pkcs8', pkcs8, algorithm, false, ['sign']);
    }
  }

  const sharedKey = keyReuse === 'shared' ? keys[0] : undefined;

  switch (mode) {
    case 'serial':
      await measureSerial(n, signParams, sharedKey);
      break;
    case 'parallel':
      await measureParallel(n, signParams, sharedKey);
      break;
  }
}
