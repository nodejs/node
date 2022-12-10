'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  len: [16, 32, 256, 1024, 1024 * 32],
  n: [1e4],
  type: ['one-byte-string', 'two-byte-string', 'ascii'],
  op: ['encode', 'encodeInto']
});

function main({ n, op, len, type }) {
  const encoder = new TextEncoder();
  let base = '';

  switch (type) {
    case 'ascii':
      base = 'a';
      break;
    case 'one-byte-string':
      base = '\xff';
      break;
    case 'two-byte-string':
      base = 'ğ';
      break;
  }

  const input = base.repeat(len);
  const subarray = new Uint8Array(len);

  bench.start();
  switch (op) {
    case 'encode': {
      for (let i = 0; i < n; i++)
        encoder.encode(input);
      break;
    }
    case 'encodeInto': {
      for (let i = 0; i < n; i++)
        encoder.encodeInto(input, subarray);
      break;
    }
  }
  bench.end(n);
}
