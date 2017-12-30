'use strict';
const common = require('../common.js');
const { URLSearchParams } = require('url');
const querystring = require('querystring');
const inputs = require('../fixtures/url-inputs.js').searchParams;

const bench = common.createBenchmark(main, {
  type: Object.keys(inputs),
  method: ['legacy', 'whatwg'],
  n: [1e6]
});

function useLegacy(n, input) {
  querystring.parse(input);
  bench.start();
  for (var i = 0; i < n; i += 1) {
    querystring.parse(input);
  }
  bench.end(n);
}

function useWHATWG(n, input) {
  new URLSearchParams(input);
  bench.start();
  for (var i = 0; i < n; i += 1) {
    new URLSearchParams(input);
  }
  bench.end(n);
}

function main({ type, n, method }) {
  const input = inputs[type];
  if (!input) {
    throw new Error('Unknown input type');
  }

  switch (method) {
    case 'legacy':
      useLegacy(n, input);
      break;
    case 'whatwg':
      useWHATWG(n, input);
      break;
    default:
      throw new Error('Unknown method');
  }
}
