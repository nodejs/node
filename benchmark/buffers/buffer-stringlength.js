'use strict';

const common = require('../common.js');
const { Buffer } = require('node:buffer');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
  encoding: ['utf8', 'latin1', 'base64'],
  len: [32, 4096, 1048576],
  input: ['ascii', 'multibyte', 'invalid'],
});

function main({ n, encoding, len, input }) {
  let buf;
  if (input === 'ascii') {
    buf = Buffer.alloc(len, 'a');
  } else {
    buf = Buffer.alloc(len - (len % 3), '€');
    if (input === 'invalid') buf = Buffer.concat([buf, Buffer.from([0xE2, 0x82])]);
  }
  const expected = buf.toString(encoding).length;
  bench.start();
  for (let i = 0; i < n; ++i) {
    assert.strictEqual(Buffer.stringLength(buf, encoding), expected);
  }
  bench.end(n);
}
