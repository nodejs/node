'use strict';

const common = require('../common.js');

const BASE = 'string\ud801';

const bench = common.createBenchmark(main, {
  len: [256, 1024, 1024 * 32],
  n: [1e4],
  op: ['encode', 'encodeInto']
});

function main({ n, op, len }) {
  const encoder = new TextEncoder();
  const input = BASE.repeat(len);
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
