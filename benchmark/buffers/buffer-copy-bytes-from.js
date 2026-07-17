'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  type: ['Uint8Array', 'Uint16Array', 'Uint32Array', 'Float64Array'],
  len: [64, 256, 2048],
  partial: ['none', 'offset', 'offset-length'],
  n: [6e5],
});

function main({ n, len, type, partial }) {
  const TypedArrayCtor = globalThis[type];
  const src = new TypedArrayCtor(len);
  for (let i = 0; i < len; i++) src[i] = i;

  let offset;
  let length;
  if (partial === 'offset') {
    offset = len >>> 2;
  } else if (partial === 'offset-length') {
    offset = len >>> 2;
    length = len >>> 1;
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    Buffer.copyBytesFrom(src, offset, length);
  }
  bench.end(n);
}
