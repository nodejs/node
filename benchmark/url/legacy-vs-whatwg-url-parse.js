'use strict';
const common = require('../common.js');
const url = require('url');
const URL = url.URL;
const assert = require('assert');

const inputs = {
  long: 'http://nodejs.org:89/docs/latest/api/url.html#test?' +
        'payload1=true&payload2=false&test=1&benchmark=3&' +
        'foo=38.38.011.293&bar=1234834910480&test=19299&3992&' +
        'key=f5c65e1e98fe07e648249ad41e1cfdb0',
  short: 'https://nodejs.org/en/blog/',
  idn: 'http://你好你好',
  auth: 'https://user:pass@example.com/path?search=1',
  special: 'file:///foo/bar/test/node.js',
  percent: 'https://%E4%BD%A0/foo',
  dot: 'https://example.org/./a/../b/./c'
};

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
