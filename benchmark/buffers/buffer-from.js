'use strict';

const common = require('../common.js');
const assert = require('assert');
const bench = common.createBenchmark(main, {
  source: [
    'array',
    'arraybuffer',
    'arraybuffer-middle',
    'buffer',
    'uint8array',
    'string',
    'string-utf8',
    'string-base64',
    'object',
  ],
  len: [10, 2048],
  n: [2048]
});

function main({ len, n, source }) {
  const array = new Array(len).fill(42);
  const arrayBuf = new ArrayBuffer(len);
  const str = 'a'.repeat(len);
  const buffer = Buffer.allocUnsafe(len);
  const uint8array = new Uint8Array(len);
  const obj = { length: null }; // Results in a new, empty Buffer

  var i;

  switch (source) {
    case 'array':
      bench.start();
      for (i = 0; i < n * 1024; i++) {
        Buffer.from(array);
      }
      bench.end(n);
      break;
    case 'arraybuffer':
      bench.start();
      for (i = 0; i < n * 1024; i++) {
        Buffer.from(arrayBuf);
      }
      bench.end(n);
      break;
    case 'arraybuffer-middle':
      const offset = ~~(len / 4);
      const length = ~~(len / 2);
      bench.start();
      for (i = 0; i < n * 1024; i++) {
        Buffer.from(arrayBuf, offset, length);
      }
      bench.end(n);
      break;
    case 'buffer':
      bench.start();
      for (i = 0; i < n * 1024; i++) {
        Buffer.from(buffer);
      }
      bench.end(n);
      break;
    case 'uint8array':
      bench.start();
      for (i = 0; i < n * 1024; i++) {
        Buffer.from(uint8array);
      }
      bench.end(n);
      break;
    case 'string':
      bench.start();
      for (i = 0; i < n * 1024; i++) {
        Buffer.from(str);
      }
      bench.end(n);
      break;
    case 'string-utf8':
      bench.start();
      for (i = 0; i < n * 1024; i++) {
        Buffer.from(str, 'utf8');
      }
      bench.end(n);
      break;
    case 'string-base64':
      bench.start();
      for (i = 0; i < n * 1024; i++) {
        Buffer.from(str, 'base64');
      }
      bench.end(n);
      break;
    case 'object':
      bench.start();
      for (i = 0; i < n * 1024; i++) {
        Buffer.from(obj);
      }
      bench.end(n);
      break;
    default:
      assert.fail(null, null, 'Should not get here');
  }
}
