'use strict';

const common = require('../common.js');
const _checkIsHttpToken = require('_http_common')._checkIsHttpToken;

const bench = common.createBenchmark(main, {
  key: [
    'TCN',
    'ETag',
    'date',
    'Vary',
    'server',
    'Server',
    'status',
    'version',
    'Expires',
    'alt-svc',
    'location',
    'Connection',
    'Keep-Alive',
    'content-type',
    'Content-Type',
    'Cache-Control',
    'Last-Modified',
    'Accept-Ranges',
    'content-length',
    'x-frame-options',
    'x-xss-protection',
    'Content-Encoding',
    'Content-Location',
    'Transfer-Encoding',
    'alternate-protocol',
    ':', // invalid input
    '@@',
    '中文呢', // unicode
    '((((())))', // invalid
    ':alternate-protocol', // fast bailout
    'alternate-protocol:' // slow bailout
  ],
  n: [5e8],
});

function main(conf) {
  var n = +conf.n;
  var key = conf.key;

  bench.start();
  for (var i = 0; i < n; i++) {
    _checkIsHttpToken(key);
  }
  bench.end(n);
}
