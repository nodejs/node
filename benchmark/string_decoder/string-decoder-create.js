'use strict';
const common = require('../common.js');
const StringDecoder = require('string_decoder').StringDecoder;

const bench = common.createBenchmark(main, {
  encoding: [
    'ascii', 'utf8', 'utf-8', 'base64', 'ucs2', 'UTF-8', 'AscII', 'UTF-16LE',
  ],
  n: [25e6],
});

function main({ encoding, n }) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    new StringDecoder(encoding);
  }
  bench.end(n);
}
