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
    'base64url',
    'BASE64URL',
    'invalid-encoding',
  ],
  n: [1e6],
}, {
  flags: ['--expose-internals'],
});

function main({ encoding, n }) {
  // Test encoding operations cache performance through Buffer.byteLength
  // which uses getEncodingOps internally
  const testString = 'test string for encoding performance';

  // Warm up the cache with some common encodings
  try {
    Buffer.byteLength(testString, 'utf8');
    Buffer.byteLength(testString, 'ascii'); 
    Buffer.byteLength(testString, 'base64');
    Buffer.byteLength(testString, 'hex');
  } catch {
    // Ignore errors for invalid encodings in warmup
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    try {
      Buffer.byteLength(testString, encoding);
    } catch {
      // For invalid encodings, the cache should still be tested
      // The error path also uses getEncodingOps
    }
  }
  bench.end(n);
}