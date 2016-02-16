'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  encoding: [ 'hex',
    'utf8',
    'utf-8',
    'ascii',
    'binary',
    'base64',
    'ucs2',
    'ucs-2',
    'utf16le',
    'utf-16le',
    'HEX',
    'UTF8',
    'UTF-8',
    'ASCII',
    'BINARY',
    'BASE64',
    'UCS2',
    'UCS-2',
    'UTF16LE',
    'UTF-16LE',
    'utf9',
    'utf-7',
    'utf17le',
    'utf-17le',
    'Unicode-FTW',
    'new gnu gun'
  ]
});

function main(conf) {
  var encoding = conf.encoding;
  bench.start();
  var n = 1024 * 1024;
  for (let i = 0; i < n; i++) {
    Buffer.isEncoding(encoding);
  }
  bench.end(n);
}
