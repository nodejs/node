'use strict';

const common = require('../common.js');
const repl = require('node:repl');
const { PassThrough, Writable } = require('node:stream');

const bench = common.createBenchmark(main, {
  n: [500],
  preview: [0, 1],
  terminal: [0, 1],
  useGlobal: [0, 1],
}, {
  combinationFilter: ({ preview, terminal }) => !!terminal || !preview,
});

function main({ n, preview, terminal, useGlobal }) {
  const inputs = Array.from({ length: n }, () => new PassThrough());
  const outputs = Array.from(
    { length: n },
    () => new Writable({ write(c, e, cb) { cb(); } }),
  );
  const servers = new Array(n);

  bench.start();
  for (let i = 0; i < n; i++) {
    servers[i] = new repl.REPLServer({
      input: inputs[i],
      output: outputs[i],
      preview: !!preview,
      terminal: !!terminal,
      useGlobal: !!useGlobal,
    });
  }
  bench.end(n);

  for (const server of servers) {
    server.close();
  }
}
