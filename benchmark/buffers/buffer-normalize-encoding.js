'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  encoding: [
    'ascii',
    'base64',
    'BASE64',
    'binary',
    'hex',
    'HEX',
    'latin1',
    'LATIN1',
    'UCS-2',
    'UCS2',
    'utf-16le',
    'UTF-16LE',
    'utf-8',
    'utf16le',
    'UTF16LE',
    'utf8',
    'UTF8',
  ],
  n: [1e6],
}, {
  flags: ['--expose-internals'],
});

function main({ encoding, n }) {
  const { normalizeEncoding } = require('internal/util');

  bench.start();
  for (let i = 0; i < n; i++) {
    normalizeEncoding(encoding);
  }
  bench.end(n);
}
