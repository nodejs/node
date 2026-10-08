'use strict';

const common = require('../common.js');
const { KeyObject } = require('crypto');

const bench = common.createBenchmark(main, {
  namedCurve: ['P-256', 'P-384', 'P-521'],
  type: ['public', 'private'],
  n: [10000],
});

async function main({ namedCurve, type, n }) {
  const pair = await crypto.subtle.generateKey({
    name: 'ECDSA', namedCurve,
  }, true, ['sign', 'verify']);
  const cryptoKey = pair[`${type}Key`];
  bench.start();
  for (let index = 0; index < n; index++) {
    if (!KeyObject.from(cryptoKey).asymmetricKeyDetails.namedCurve)
      throw new Error('Missing named curve');
  }
  bench.end(n);
}
