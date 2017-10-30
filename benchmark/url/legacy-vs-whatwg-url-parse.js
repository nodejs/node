'use strict';
const common = require('../common.js');
const url = require('url');
const URL = url.URL;
const assert = require('assert');
const inputs = require('../fixtures/url-inputs.js').urls;

const bench = common.createBenchmark(main, {
  type: Object.keys(inputs),
  method: ['legacy', 'whatwg'],
  n: [1e5]
});

function useLegacy(n, input) {
  var noDead = url.parse(input);
  bench.start();
  for (var i = 0; i < n; i += 1) {
    noDead = url.parse(input);
  }
  bench.end(n);
  return noDead;
}

function useWHATWG(n, input) {
  var noDead = new URL(input);
  bench.start();
  for (var i = 0; i < n; i += 1) {
    noDead = new URL(input);
  }
  bench.end(n);
  return noDead;
}

function main(conf) {
  const type = conf.type;
  const n = conf.n | 0;
  const method = conf.method;

  const input = inputs[type];
  if (!input) {
    throw new Error('Unknown input type');
  }

  var noDead;  // Avoid dead code elimination.
  switch (method) {
    case 'legacy':
      noDead = useLegacy(n, input);
      break;
    case 'whatwg':
      noDead = useWHATWG(n, input);
      break;
    default:
      throw new Error('Unknown method');
  }

  assert.ok(noDead);
}
