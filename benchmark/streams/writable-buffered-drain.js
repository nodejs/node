'use strict';

const common = require('../common');
const { Writable } = require('stream');

const bench = common.createBenchmark(main, {
  count: [1024, 16384, 65536],
  n: [100],
});

function main({ count, n }) {
  const chunk = {};
  let callback;
  const stream = new Writable({
    objectMode: true,
    write(chunk, encoding, cb) { callback = cb; },
  });

  bench.start();
  for (let i = 0; i < n; i++) {
    for (let j = 0; j < count; j++) stream.write(chunk);
    for (let j = 0; j < count; j++) callback();
  }
  bench.end(n);
}
