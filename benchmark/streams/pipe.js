'use strict';

const common = require('../common');
const { Readable, Writable } = require('stream');

const bench = common.createBenchmark(main, {
  n: [5e6]
});

function main({ n }) {
  const b = Buffer.alloc(1024);
  const r = new Readable();
  const w = new Writable();

  let i = 0;

  r._read = () => r.push(i++ === n ? null : b);
  w._write = (data, enc, cb) => cb();

  bench.start();

  r.pipe(w);
  w.on('finish', () => bench.end(n));
}
