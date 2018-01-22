'use strict';

const common = require('../common.js');
const _checkInvalidHeaderChar = require('_http_common')._checkInvalidHeaderChar;

const bench = common.createBenchmark(main, {
  key: [
    // Valid
    '',
    '1',
    '\t\t\t\t\t\t\t\t\t\tFoo bar baz',
    'keep-alive',
    'close',
    'gzip',
    '20091',
    'private',
    'text/html; charset=utf-8',
    'text/plain',
    'Sat, 07 May 2016 16:54:48 GMT',
    'SAMEORIGIN',
    'en-US',

    // Invalid
    'Here is a value that is really a folded header value\r\n  this should be \
     supported, but it is not currently',
    '中文呢', // unicode
    'foo\nbar',
    '\x7F'
  ],
  n: [1e6],
});

function main({ n, key }) {
  bench.start();
  for (var i = 0; i < n; i++) {
    _checkInvalidHeaderChar(key);
  }
  bench.end(n);
}
