'use strict';

const common = require('../common');
const Transform = require('stream').Transform;

const bench = common.createBenchmark(main, {
  n: [2e6],
  sync: ['yes', 'no'],
  callback: ['yes', 'no']
});

function main({ n, sync, callback }) {
  const b = Buffer.allocUnsafe(1024);
  const s = new Transform();
  sync = sync === 'yes';

  const writecb = (cb) => {
    if (sync)
      cb();
    else
      process.nextTick(cb);
  };

  s._transform = (chunk, encoding, cb) => writecb(cb);

  const cb = callback === 'yes' ? () => {} : null;

  bench.start();

  let k = 0;
  function run() {
    while (k++ < n && s.write(b, cb));
    if (k >= n)
      bench.end(n);
  }
  s.on('drain', run);
  s.on('data', () => {});
  run();
}
