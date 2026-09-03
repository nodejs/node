'use strict';

const common = require('../common.js');
const repl = require('node:repl');
const { PassThrough, Writable } = require('node:stream');

const bench = common.createBenchmark(main, {
  n: [1e3],
});

function main({ n }) {
  const input = new PassThrough();
  const output = new Writable({ write(c, e, cb) { cb(); } });
  const server = new repl.REPLServer({
    input,
    output,
    terminal: false,
  });

  bench.start();
  for (let i = 0; i < n; i++) {
    server.resetContext();
  }
  bench.end(n);
  server.close();
}
