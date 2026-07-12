'use strict';

const common = require('../common.js');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../../test/common/tmpdir');

const bench = common.createBenchmark(main, {
  size: [64, 1024, 16384, 262144, 4194304],
  content: ['ascii', 'latin1', 'utf8_mixed'],
  source: ['path', 'fd'],
  n: [3e3],
});

function buildContent(kind, size) {
  if (kind === 'ascii') {
    return Buffer.alloc(size, 0x61); // 'a'
  }
  if (kind === 'latin1') {
    // 'é' in UTF-8 is 0xC3 0xA9 (2 bytes per char)
    const pair = Buffer.from([0xC3, 0xA9]);
    const buf = Buffer.alloc(size);
    for (let i = 0; i + 2 <= size; i += 2) pair.copy(buf, i);
    return buf;
  }
  if (kind === 'utf8_mixed') {
    // mixed ASCII + 3-byte CJK (U+4E2D 中 = E4 B8 AD)
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
  throw new Error('unknown content: ' + kind);
}

function main({ n, size, content, source }) {
  tmpdir.refresh();
  const file = path.join(tmpdir.path, `bench-${content}-${size}.bin`);
  fs.writeFileSync(file, buildContent(content, size));

  let arg;
  let shouldClose = false;
  if (source === 'fd') {
    arg = fs.openSync(file, 'r');
    shouldClose = true;
  } else {
    arg = file;
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    fs.readFileSync(arg, 'utf8');
  }
  bench.end(n);

  if (shouldClose) fs.closeSync(arg);
}
