'use strict';

const common = require('../common');
const Writable = require('stream').Writable;

const bench = common.createBenchmark(main, {
  n: [2e6],
  sync: ['yes', 'no']
});

function main({ n, sync }) {
  const b = Buffer.allocUnsafe(1024);
  const s = new Writable();
  sync = sync === 'yes';
  s._write = function(chunk, encoding, cb) {
    if (sync)
      cb();
    else
      process.nextTick(cb);
  };

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
