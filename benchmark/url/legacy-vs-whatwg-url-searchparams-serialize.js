'use strict';
const common = require('../common.js');
const querystring = require('querystring');
const searchParams = common.searchParams;

const bench = common.createBenchmark(main, {
  searchParam: Object.keys(searchParams),
  method: ['legacy', 'whatwg'],
  n: [1e6],
});

function useLegacy(n, input, prop) {
  const obj = querystring.parse(input);
  querystring.stringify(obj);
  bench.start();
  for (let i = 0; i < n; i += 1) {
    querystring.stringify(obj);
  }
  bench.end(n);
}

function useWHATWG(n, param, prop) {
  const obj = new URLSearchParams(param);
  obj.toString();
  bench.start();
  for (let i = 0; i < n; i += 1) {
    obj.toString();
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
