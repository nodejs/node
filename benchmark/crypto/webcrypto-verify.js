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

let publicKeys;
let signature;

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

async function measureSerial(n, verifyParams, sharedKey) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    await subtle.verify(verifyParams, sharedKey || publicKeys[i], signature, data);
  }
  bench.end(n);
}

async function measureParallel(n, verifyParams, sharedKey) {
  const promises = new Array(n);
  bench.start();
  for (let i = 0; i < n; ++i) {
    promises[i] = subtle.verify(verifyParams, sharedKey || publicKeys[i], signature, data);
  }
  await Promise.all(promises);
  bench.end(n);
}

async function main({ n, mode, keyReuse, keyType }) {
  const algorithm = kAlgorithms[keyType];
  const verifyParams = kSignParams[keyType];

  if (!publicKeys || publicKeys.length !== n ||
      publicKeys[0].algorithm.name !== verifyParams.name) {
    publicKeys = new Array(n);
    // Generate one key pair, then import its spki bytes n times to get
    // distinct CryptoKey instances.
    const kp = await subtle.generateKey(algorithm, true, ['sign', 'verify']);
    const spki = await subtle.exportKey('spki', kp.publicKey);
    for (let i = 0; i < n; ++i) {
      publicKeys[i] = await subtle.importKey('spki', spki, algorithm, false, ['verify']);
    }
    signature = await subtle.sign(verifyParams, kp.privateKey, data);
  }

  const sharedKey = keyReuse === 'shared' ? publicKeys[0] : undefined;

  switch (mode) {
    case 'serial':
      await measureSerial(n, verifyParams, sharedKey);
      break;
    case 'parallel':
      await measureParallel(n, verifyParams, sharedKey);
      break;
  }
}
