'use strict';

const common = require('../common');
const Writable = require('stream').Writable;

const bench = common.createBenchmark(main, {
  n: [2e6],
  sync: ['yes', 'no'],
  writev: ['yes', 'no']
});

function main({ n, sync, writev }) {
  const b = Buffer.allocUnsafe(1024);
  const s = new Writable();
  sync = sync === 'yes';

  if (writev === 'yes') {
    s._writev = function(chunks, cb) {
      if (sync)
        cb();
      else
        process.nextTick(cb);
    };
  } else {
    s._write = function(chunk, encoding, cb) {
      if (sync)
        cb();
      else
        process.nextTick(cb);
    };
  }

  bench.start();

  let k = 0;
  function run() {
    while (k++ < n && s.write(b));
    if (k >= n)
      bench.end(n);
  }
  s.on('drain', run);
  run();
}
