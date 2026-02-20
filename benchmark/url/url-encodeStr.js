'use strict';

const common = require('../common.js');
const { encodeStr } = require('internal/querystring');

const noEscape = new Int8Array([
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x00 - 0x0F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x10 - 0x1F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, // 0x20 - 0x2F
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, // 0x30 - 0x3F
  0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x40 - 0x4F
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, // 0x50 - 0x5F
  0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x60 - 0x6F
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, // 0x70 - 0x7F
]);

const hexTable = new Array(256);
for (let i = 0; i < 256; i++) {
  hexTable[i] = '%' + i.toString(16).toUpperCase().padStart(2, '0');
}

const bench = common.createBenchmark(main, {
  len: [5, 50],
  n: [1e7],
});

function main({ n, len }) {
  const str = 'a'.repeat(len - 1) + ' '; // One char that needs escaping

  // Warm up
  encodeStr(str, noEscape, hexTable);

  bench.start();
  for (let i = 0; i < n; i++) {
    encodeStr(str, noEscape, hexTable);
  }
  bench.end(n);
}
