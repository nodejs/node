'use strict';
const common = require('../common.js');
const { URLSearchParams } = require('url');

const bench = common.createBenchmark(main, {
  method: ['get', 'getAll', 'has'],
  param: ['one', 'two', 'three', 'nonexistent'],
  n: [2e7],
});

const str = 'one=single&two=first&three=first&two=2nd&three=2nd&three=3rd';

function main({ method, param, n }) {
  const params = new URLSearchParams(str);
  const fn = params[method];
  if (!fn)
    throw new Error(`Unknown method ${method}`);

  bench.start();
  for (var i = 0; i < n; i += 1)
    fn(param);
  bench.end(n);
}
