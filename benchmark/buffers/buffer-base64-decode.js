'use strict';
const assert = require('assert');
const common = require('../common.js');

const bench = common.createBenchmark(main, {});

function main(conf) {
  const s = 'abcd'.repeat(8 << 20);
  s.match(/./);  // Flatten string.
  assert.equal(s.length % 4, 0);
  const b = Buffer(s.length / 4 * 3);
  b.write(s, 0, s.length, 'base64');
  bench.start();
  for (var i = 0; i < 32; i += 1) b.base64Write(s, 0, s.length);
  bench.end(32);
}
