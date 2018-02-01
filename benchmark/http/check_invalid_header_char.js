'use strict';

const common = require('../common.js');
const _checkInvalidHeaderChar = require('_http_common')._checkInvalidHeaderChar;

// Put it here so the benchmark result lines will not be super long.
const LONG_AND_INVALID = 'Here is a value that is really a folded header ' +
  'value\r\n  this should be \supported, but it is not currently';

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
    'LONG_AND_INVALID',
    '中文呢', // unicode
    'foo\nbar',
    '\x7F'
  ],
  n: [1e6],
});

function main({ n, key }) {
  if (key === 'LONG_AND_INVALID') {
    key = LONG_AND_INVALID;
  }
  bench.start();
  for (var i = 0; i < n; i++) {
    _checkInvalidHeaderChar(key);
  }
  bench.end(n);
}
