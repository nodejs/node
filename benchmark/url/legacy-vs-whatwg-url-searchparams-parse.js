'use strict';
const common = require('../common.js');
const { URLSearchParams } = require('url');
const querystring = require('querystring');
const searchParams = common.searchParams;

const bench = common.createBenchmark(main, {
  searchParam: Object.keys(searchParams),
  method: ['legacy', 'whatwg'],
  n: [1e6]
});

function useLegacy(n, input) {
  querystring.parse(input);
  bench.start();
  for (let i = 0; i < n; i += 1) {
    querystring.parse(input);
  }
  bench.end(n);
}

function useWHATWG(n, param) {
  new URLSearchParams(param);
  bench.start();
  for (let i = 0; i < n; i += 1) {
    new URLSearchParams(param);
  }
  bench.end(n);
}

function main({ searchParam, n, method }) {
  const param = searchParams[searchParam];
  if (!param) {
    throw new Error(`Unknown search parameter type "${searchParam}"`);
  }

  switch (method) {
    case 'legacy':
      useLegacy(n, param);
      break;
    case 'whatwg':
      useWHATWG(n, param);
      break;
    default:
      throw new Error(`Unknown method ${method}`);
  }
}
