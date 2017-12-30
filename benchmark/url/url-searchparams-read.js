'use strict';
const common = require('../common.js');
const { URLSearchParams } = require('url');

const bench = common.createBenchmark(main, {
  method: ['get', 'getAll', 'has'],
  param: ['one', 'two', 'three', 'nonexistent'],
  n: [2e7]
});

const str = 'one=single&two=first&three=first&two=2nd&three=2nd&three=3rd';

function get(n, param) {
  const params = new URLSearchParams(str);

  bench.start();
  for (var i = 0; i < n; i += 1)
    params.get(param);
  bench.end(n);
}

function getAll(n, param) {
  const params = new URLSearchParams(str);

  bench.start();
  for (var i = 0; i < n; i += 1)
    params.getAll(param);
  bench.end(n);
}

function has(n, param) {
  const params = new URLSearchParams(str);

  bench.start();
  for (var i = 0; i < n; i += 1)
    params.has(param);
  bench.end(n);
}

function main({ method, param, n }) {
  switch (method) {
    case 'get':
      get(n, param);
      break;
    case 'getAll':
      getAll(n, param);
      break;
    case 'has':
      has(n, param);
      break;
    default:
      throw new Error('Unknown method');
  }
}
