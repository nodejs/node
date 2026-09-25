'use strict';

const common = require('../common.js');
const { subtle } = globalThis.crypto;

const signParams = { name: 'HMAC' };

let keys;
let currentHash;
let currentKeyLength;

const bench = common.createBenchmark(main, {
  hash: ['SHA-1', 'SHA-256', 'SHA-512'],
  mode: ['serial', 'parallel'],
  keyReuse: ['shared', 'unique'],
  keylen: [512],
  len: [0, 256, 4096],
  n: [1e3],
}, {
  test: {
    keylen: 512,
  },
  combinationFilter(p) {
    // Unique only differs from shared when operations overlap (parallel);
    // sequential calls have no contention so unique+serial adds no value.
    if (p.keyReuse === 'unique') return p.mode === 'parallel';
    return true;
  },
});

async function createSigningKeys(n, hash, keylen) {
  keys = new Array(n);
  currentHash = hash;
  currentKeyLength = keylen;

  const algorithm = { name: 'HMAC', hash, length: keylen };
  const key = await subtle.generateKey(algorithm, true, ['sign']);
  const raw = await subtle.exportKey('raw', key);
  for (let i = 0; i < n; ++i) {
    keys[i] = await subtle.importKey('raw', raw, algorithm, false, ['sign']);
  }
}

async function measureSerial(n, sharedKey, data) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    await subtle.sign(signParams, sharedKey || keys[i], data);
  }
  bench.end(n);
}

async function measureParallel(n, sharedKey, data) {
  const promises = new Array(n);
  bench.start();
  for (let i = 0; i < n; ++i) {
    promises[i] = subtle.sign(signParams, sharedKey || keys[i], data);
  }
  await Promise.all(promises);
  bench.end(n);
}

async function main({ n, mode, keyReuse, hash, keylen, len }) {
  if (!keys || keys.length !== n ||
      currentHash !== hash || currentKeyLength !== keylen) {
    await createSigningKeys(n, hash, keylen);
  }

  const data = new Uint8Array(len);
  data.fill(0x62);
  const sharedKey = keyReuse === 'shared' ? keys[0] : undefined;

  switch (mode) {
    case 'serial':
      await measureSerial(n, sharedKey, data);
      break;
    case 'parallel':
      await measureParallel(n, sharedKey, data);
      break;
  }
}
