'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  bytes: [8, 128, 1024, 16384],
  type: ['buffer', 'uint8array', 'arraybuffer'],
  partial: ['true', 'false'],
  n: [5e5],
});

function main({ n, bytes, type, partial }) {
  let source, target;

  switch (type) {
    case 'buffer':
      source = Buffer.allocUnsafe(bytes);
      target = Buffer.allocUnsafe(bytes);
      break;
    case 'uint8array':
      source = new Uint8Array(bytes);
      target = new Uint8Array(bytes);
      break;
    case 'arraybuffer':
      source = new ArrayBuffer(bytes);
      target = new ArrayBuffer(bytes);
      break;
  }

  const sourceStart = (partial === 'true' ? Math.floor(bytes / 2) : 0);
  const sourceEnd = bytes;

  bench.start();
  for (let i = 0; i < n; i++) {
    Buffer.copy(source, target, 0, sourceStart, sourceEnd);
  }
  bench.end(n);
}
