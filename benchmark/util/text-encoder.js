'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  len: [0, 256, 1024, 1024 * 32],
  n: [1e4],
  type: ['v8-one-byte-string', 'v8-two-byte-string'],
  op: ['encode', 'encodeInto']
});

function main({ n, op, len, type }) {
  const encoder = new TextEncoder();
  let base = '';

  switch (type) {
    case 'v8-one-byte-string':
      base = 'a';
      break;
    case 'v8-two-byte-string':
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
