'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  bytes: [8, 128, 1024, 16384],
  type: ['buffer', 'uint8array', 'arraybuffer'],
  partial: ['true', 'false'],
  n: [10000],
});

function main({ n, bytes, type, partial }) {
  let source, target, srcView, dstBuffer;

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

  const srcOffset = (partial === 'true' ? Math.floor(bytes / 2) : 0);
  const length = bytes - srcOffset;
  const dstOffset = 0;

  if (type === 'arraybuffer') {
    srcView = new Uint8Array(source, srcOffset, length);
    dstBuffer = new Uint8Array(target);
  } else {
    srcView = new Uint8Array(
      source.buffer || source,
      source.byteOffset !== undefined ? source.byteOffset + srcOffset : srcOffset,
      length
    );
    dstBuffer = new Uint8Array(target.buffer || target, target.byteOffset || 0);
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    dstBuffer.set(srcView, dstOffset);
  }
  bench.end(n);
}
