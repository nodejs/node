'use strict';

const common = require('../common.js');
const repl = require('node:repl');
const { PassThrough, Writable } = require('node:stream');

const bench = common.createBenchmark(main, {
  n: [5e3],
  query: [
    'cons',
    'console.lo',
    'Buffer.prototype.wri',
    "require('f",
  ],
  useGlobal: [0, 1],
});

function main({ n, query, useGlobal }) {
  const input = new PassThrough();
  const output = new Writable({ write(c, e, cb) { cb(); } });
  const server = new repl.REPLServer({
    input,
    output,
    terminal: false,
    useGlobal: !!useGlobal,
  });

  // Inspector callbacks do not keep the event loop alive on their own.
  const keepAlive = setInterval(() => {}, 0x7fffffff);
  let remaining = n;

  function complete() {
    server.complete(query, onComplete);
  }

  function onComplete(err) {
    if (err) {
      throw err;
    }

    if (--remaining === 0) {
      bench.end(n);
      clearInterval(keepAlive);
      server.close();
      return;
    }

    setImmediate(complete);
  }

  bench.start();
  setImmediate(complete);
}
