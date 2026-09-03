'use strict';

// buf.write(string, 'utf8') for strings whose in-memory representation is
// one-byte (Latin-1) or two-byte (UTF-16), which take different encoder paths.
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  chars: ['one-byte', 'two-byte', 'two-byte-astral', 'two-byte-lone-surrogate'],
  len: [16, 256, 2048, 65536],
  n: [5e5],
});

function makeString(chars, len) {
  switch (chars) {
    case 'one-byte':
      return 'aé'.repeat(len / 2);
    case 'two-byte':
      return 'aé€日'.repeat(len / 4);
    case 'two-byte-astral':
      return 'aé€日\u{1F600}'.repeat(len / 6).padEnd(len, 'a');
    case 'two-byte-lone-surrogate':
      return 'aé€日'.repeat(len / 4 - 1) + 'ab\ud800c';
    default:
      throw new Error(chars);
  }
}

function main({ chars, len, n }) {
  const string = makeString(chars, len);
  const buf = Buffer.allocUnsafe(Buffer.byteLength(string));
  if (len >= 65536) n = Math.floor(n / 32);
  bench.start();
  for (let i = 0; i < n; ++i) {
    buf.write(string, 0, 'utf8');
  }
  bench.end(n);
}
