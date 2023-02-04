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
    'uint8array',
    'uint16array',
  ],
  len: [100, 2048],
  n: [8e5],
});

function main({ len, n, source }) {
  let i = 0;

  switch (source) {
    case 'array': {
      const array = new Array(len).fill(42);
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(array);
      }
      bench.end(n);
      break;
    }
    case 'arraybuffer': {
      const arrayBuf = new ArrayBuffer(len);
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(arrayBuf);
      }
      bench.end(n);
      break;
    }
    case 'arraybuffer-middle': {
      const arrayBuf = new ArrayBuffer(len);
      const offset = ~~(len / 4);
      const length = ~~(len / 2);
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(arrayBuf, offset, length);
      }
      bench.end(n);
      break;
    }
    case 'buffer': {
      const buffer = Buffer.allocUnsafe(len);
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(buffer);
      }
      bench.end(n);
      break;
    }
    case 'uint8array': {
      const uint8array = new Uint8Array(len);
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(uint8array);
      }
      bench.end(n);
      break;
    }
    case 'uint16array': {
      const uint16array = new Uint16Array(len);
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(uint16array);
      }
      bench.end(n);
      break;
    }
    case 'string': {
      const str = 'a'.repeat(len);
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(str);
      }
      bench.end(n);
      break;
    }
    case 'string-utf8': {
      const str = 'a'.repeat(len);
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(str, 'utf8');
      }
      bench.end(n);
      break;
    }
    case 'string-base64': {
      const str = 'a'.repeat(len);
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(str, 'base64');
      }
      bench.end(n);
      break;
    }
    case 'object': {
      const obj = { length: null }; // Results in a new, empty Buffer
      bench.start();
      for (i = 0; i < n; i++) {
        Buffer.from(obj);
      }
      bench.end(n);
      break;
    }
    default:
      assert.fail('Should not get here');
  }
}
