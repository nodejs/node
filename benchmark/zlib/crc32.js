'use strict';

const common = require('../common.js');
const { crc32 } = require('zlib');

// Benchmark crc32 on Buffer and String inputs across sizes.
// Iteration count is scaled inversely with input length to keep runtime sane.
// Example:
//   node benchmark/zlib/crc32.js type=buffer len=4096 n=4000000
//   ./out/Release/node benchmark/zlib/crc32.js --test

const bench = common.createBenchmark(main, {
  type: ['buffer', 'string'],
  len: [32, 256, 4096, 65536],
  n: [4e6],
});

function makeBuffer(size) {
  const buf = Buffer.allocUnsafe(size);
  for (let i = 0; i < size; i++) buf[i] = (i * 1103515245 + 12345) & 0xff;
  return buf;
}

function makeAsciiString(size) {
  const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';
  let out = '';
  for (let i = 0, j = 0; i < size; i++, j = (j + 7) % chars.length) out += chars[j];
  return out;
}

function main({ type, len, n }) {
  // Scale iterations so that total processed bytes roughly constant around n*4096 bytes.
  const scale = 4096 / len;
  const iters = Math.max(1, Math.floor(n * scale));

  const data = type === 'buffer' ? makeBuffer(len) : makeAsciiString(len);

  let acc = 0;
  for (let i = 0; i < Math.min(iters, 10000); i++) acc ^= crc32(data, 0);

  bench.start();
  let sum = 0;
  for (let i = 0; i < iters; i++) sum ^= crc32(data, 0);
  bench.end(iters);

  if (sum === acc - 1) process.stderr.write('');
}
