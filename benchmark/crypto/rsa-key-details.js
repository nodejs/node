'use strict';

const common = require('../common.js');
const { KeyObject } = require('crypto');

const bench = common.createBenchmark(main, {
  type: ['public', 'private'],
  n: [10000],
});

async function main({ type, n }) {
  const pair = await crypto.subtle.generateKey({
    name: 'RSA-PSS',
    modulusLength: 2048,
    publicExponent: new Uint8Array([1, 0, 1]),
    hash: 'SHA-256',
  }, true, ['sign', 'verify']);
  const cryptoKey = pair[`${type}Key`];
  // Use a fresh KeyObject so each iteration retrieves uncached key details.
  bench.start();
  for (let index = 0; index < n; index++) {
    if (KeyObject.from(cryptoKey).asymmetricKeyDetails.modulusLength !== 2048)
      throw new Error('Unexpected modulus length');
  }
  bench.end(n);
}
