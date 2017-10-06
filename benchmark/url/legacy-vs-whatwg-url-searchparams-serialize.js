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

function useLegacy(n, input, prop) {
  const obj = querystring.parse(input);
  querystring.stringify(obj);
  bench.start();
  for (var i = 0; i < n; i += 1) {
    querystring.stringify(obj);
  }
  bench.end(n);
}

function useWHATWG(n, input, prop) {
  const obj = new URLSearchParams(input);
  obj.toString();
  bench.start();
  for (var i = 0; i < n; i += 1) {
    obj.toString();
  }
  bench.end(n);
}

function main(conf) {
  const type = conf.type;
  const n = conf.n | 0;
  const method = conf.method;

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
