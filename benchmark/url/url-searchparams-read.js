'use strict';
const common = require('../common.js');
const { URLSearchParams } = require('url');

const bench = common.createBenchmark(main, {
  accessMethod: ['get', 'getAll', 'has'],
  param: ['one', 'two', 'three', 'nonexistent'],
  n: [2e7]
});

const str = 'one=single&two=first&three=first&two=2nd&three=2nd&three=3rd';

function main({ accessMethod, param, n }) {
  const params = new URLSearchParams(str);
  if (!params[accessMethod])
    throw new Error(`Unknown method ${accessMethod}`);

  bench.start();
  for (var i = 0; i < n; i += 1)
    params[accessMethod](param);
  bench.end(n);
}
