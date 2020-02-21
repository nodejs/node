'use strict';

const common = require('../common');
const Writable = require('stream').Writable;

const bench = common.createBenchmark(main, {
  n: [2e6],
  sync: ['yes', 'no'],
  writev: ['yes', 'no'],
  callback: ['yes', 'no']
});

function main({ n, sync, writev, callback }) {
  const b = Buffer.allocUnsafe(1024);
  const s = new Writable();
  sync = sync === 'yes';

  const writecb = (cb) => {
    if (sync)
      cb();
    else
      process.nextTick(cb);
  };

  if (writev === 'yes') {
    s._writev = (chunks, cb) => writecb(cb);
  } else {
    s._write = (chunk, encoding, cb) => writecb(cb);
  }

  const cb = callback === 'yes' ? () => {} : null;

  bench.start();

  let k = 0;
  function run() {
    while (k++ < n && s.write(b, cb));
    if (k >= n) {
      bench.end(n);
      s.removeListener('drain', run);
    }
  }
  s.on('drain', run);
  run();
}
