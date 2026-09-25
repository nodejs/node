'use strict';
const common = require('../common.js');
const url = require('url');
const URL = url.URL;

const bench = common.createBenchmark(main, {
  type: ['valid', 'invalid'],
  e: [1e5],
});

// This benchmark is used to compare the `Invalid URL` path of the URL parser
function main({ type, e }) {
  const url = type === 'valid' ? 'https://www.nodejs.org' : 'www.nodejs.org';
  bench.start();
  for (let i = 0; i < e; i++) {
    try {
      new URL(url);
    } catch {
      // do nothing
    }
  }
  bench.end(e);
}
