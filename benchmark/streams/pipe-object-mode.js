'use strict';

const common = require('../common');
const { Readable, Writable } = require('stream');

const bench = common.createBenchmark(main, {
  n: [5e6]
});

function main({ n }) {
  const b = {};
  const r = new Readable({ objectMode: true });
  const w = new Writable({ objectMode: true });

  var i = 0;

  r._read = () => r.push(i++ === n ? null : b);
  w._write = (data, enc, cb) => cb();

  bench.start();

  r.pipe(w);
  w.on('finish', () => bench.end(n));
}
