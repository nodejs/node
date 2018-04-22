'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  charsPerLine: [76],
  linesCount: [8 << 16],
  n: [32],
});

function main({ charsPerLine, linesCount, n }) {
  const bytesCount = charsPerLine * linesCount / 4 * 3;

  const line = `${'abcd'.repeat(charsPerLine / 4)}\n`;
  const data = line.repeat(linesCount);
  // eslint-disable-next-line node-core/no-unescaped-regexp-dot
  data.match(/./);  // Flatten the string
  const buffer = Buffer.alloc(bytesCount, line, 'base64');

  bench.start();
  for (var i = 0; i < n; i++) {
    buffer.base64Write(data, 0, bytesCount);
  }
  bench.end(n);
}
