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

function useLegacy(n, input, prop) {
  const obj = url.parse(input);
  var noDead = url.format(obj);
  bench.start();
  for (var i = 0; i < n; i += 1) {
    noDead = url.format(obj);
  }
  bench.end(n);
  return noDead;
}

function useWHATWG(n, input, prop) {
  const obj = new URL(input);
  var noDead = obj.toString();
  bench.start();
  for (var i = 0; i < n; i += 1) {
    noDead = obj.toString();
  }
  bench.end(n);
  return noDead;
}

function main({ type, n, method }) {
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
