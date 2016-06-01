'use strict';
const common = require('../common.js');
const StringDecoder = require('string_decoder').StringDecoder;

const bench = common.createBenchmark(main, {
  encoding: [
    'ascii', 'utf8', 'utf-8', 'base64', 'ucs2', 'UTF-8', 'AscII', 'UTF-16LE'
  ],
  n: [25e6]
});

function main(conf) {
  const encoding = conf.encoding;
  const n = conf.n | 0;

  bench.start();
  for (var i = 0; i < n; ++i) {
    const sd = new StringDecoder(encoding);
    !!sd.encoding;
  }
  bench.end(n);
}
