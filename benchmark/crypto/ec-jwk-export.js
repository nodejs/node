'use strict';

const common = require('../common.js');
const { generateKeyPairSync } = require('crypto');

const bench = common.createBenchmark(main, {
  namedCurve: ['prime256v1', 'secp384r1', 'secp521r1', 'secp256k1'],
  type: ['public', 'private'],
  n: [10000],
});

function main({ namedCurve, type, n }) {
  const key = generateKeyPairSync('ec', { namedCurve })[`${type}Key`];
  const options = { format: 'jwk' };
  bench.start();
  for (let index = 0; index < n; index++)
    key.export(options);
  bench.end(n);
}
