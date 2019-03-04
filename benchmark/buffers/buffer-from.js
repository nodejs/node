'use strict';

const common = require('../common.js');
const assert = require('assert');
const bench = common.createBenchmark(main, {
  source: [
    'array',
    'arraybuffer',
    'arraybuffer-middle',
    'buffer',
    'string',
    'string-utf8',
    'string-base64',
    'object',
  ],
  len: [100, 2048],
  n: [8e5]
});

function main({ len, n, source }) {
  const array = new Array(len).fill(42);
  const arrayBuf = new ArrayBuffer(len);
  const str = 'a'.repeat(len);
  const buffer = Buffer.allocUnsafe(len);
  const uint8array = new Uint8Array(len);
  const obj = { length: null }; // Results in a new, empty Buffer

  let i = 0;

  switch (source) {
    case 'array':
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(array);
      }
      bench.end(n);
      break;
    case 'arraybuffer':
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(arrayBuf);
      }
      bench.end(n);
      break;
    case 'arraybuffer-middle':
      const offset = ~~(len / 4);
      const length = ~~(len / 2);
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(arrayBuf, offset, length);
      }
      bench.end(n);
      break;
    case 'buffer':
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(buffer);
      }
      bench.end(n);
      break;
    case 'uint8array':
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(uint8array);
      }
      bench.end(n);
      break;
    case 'string':
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(str);
      }
      bench.end(n);
      break;
    case 'string-utf8':
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(str, 'utf8');
      }
      bench.end(n);
      break;
    case 'string-base64':
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(str, 'base64');
      }
      bench.end(n);
      break;
    case 'object':
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(obj);
      }
      bench.end(n);
      break;
    default:
      assert.fail('Should not get here');
  }
}
