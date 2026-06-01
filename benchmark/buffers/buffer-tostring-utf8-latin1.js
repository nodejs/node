'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  size: [64, 1024, 16384, 262144, 4194304],
  content: ['ascii', 'latin1', 'utf8_mixed', 'latin1_then_cjk'],
  n: [1e4],
});

function buildBuffer(kind, size) {
  if (kind === 'ascii') {
    return Buffer.alloc(size, 0x61);
  }
  if (kind === 'latin1') {
    const pair = Buffer.from([0xC3, 0xA9]);
    const buf = Buffer.alloc(size);
    for (let i = 0; i + 2 <= size; i += 2) pair.copy(buf, i);
    return buf;
  }
  if (kind === 'utf8_mixed') {
    const cjk = Buffer.from([0xE4, 0xB8, 0xAD]);
    const buf = Buffer.alloc(size);
    let i = 0;
    while (i + 4 <= size) {
      buf[i++] = 0x61;
      cjk.copy(buf, i);
      i += 3;
    }
    return buf;
  }
  if (kind === 'latin1_then_cjk') {
    const pair = Buffer.from([0xC3, 0xA9]);
    const cjk = Buffer.from([0xE4, 0xB8, 0xAD]);
    const buf = Buffer.alloc(size);
    const mid = (size >> 1) & ~1;
    for (let i = 0; i + 2 <= mid; i += 2) pair.copy(buf, i);
    cjk.copy(buf, mid);
    for (let i = mid + 3; i + 2 <= size; i += 2) pair.copy(buf, i);
    return buf;
  }
  throw new Error('unknown content: ' + kind);
}

function main({ n, size, content }) {
  const buf = buildBuffer(content, size);

  bench.start();
  for (let i = 0; i < n; i++) {
    buf.toString('utf8');
  }
  bench.end(n);
}
