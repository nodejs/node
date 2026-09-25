'use strict';

const common = require('../common.js');
const { generateKeyPairSync } = require('crypto');

const bench = common.createBenchmark(main, {
  namedCurve: ['prime256v1', 'secp384r1', 'secp521r1'],
  format: ['raw-private', 'raw-public'],
  type: ['uncompressed', 'compressed'],
  n: [10000],
}, {
  combinationFilter: ({ format, type }) => format === 'raw-public' || type === 'uncompressed',
});

function main({ namedCurve, format, type, n }) {
  const pair = generateKeyPairSync('ec', { namedCurve });
  const key = format === 'raw-private' ? pair.privateKey : pair.publicKey;
  const options = format === 'raw-public' ? { format, type } : { format };
  bench.start();
  for (let index = 0; index < n; index++)
    key.export(options);
  bench.end(n);
}
