'use strict';

const common = require('../common.js');
const repl = require('node:repl');
const { PassThrough, Writable } = require('node:stream');

const bench = common.createBenchmark(main, {
  n: [2e4],
  code: [
    '1 + 1',
    '({ answer: 42 })',
    'Promise.resolve(42)',
    'await Promise.resolve(42)',
  ],
  mode: ['sloppy', 'strict'],
  useGlobal: [0, 1],
});

function main({ n, code, mode, useGlobal }) {
  const input = new PassThrough();
  const output = new Writable({ write(c, e, cb) { cb(); } });
  const server = new repl.REPLServer({
    input,
    output,
    replMode: mode === 'strict' ?
      repl.REPL_MODE_STRICT :
      repl.REPL_MODE_SLOPPY,
    terminal: false,
    useGlobal: !!useGlobal,
  });

  // Inspector callbacks do not keep the event loop alive on their own.
  const keepAlive = setInterval(() => {}, 0x7fffffff);
  let remaining = n;

  function evaluate() {
    server.eval(`${code}\n`, server.context, 'repl', onEvaluate);
  }

  function onEvaluate(err) {
    if (err) {
      throw err;
    }

    if (--remaining === 0) {
      bench.end(n);
      clearInterval(keepAlive);
      server.close();
      return;
    }

    setImmediate(evaluate);
  }

  bench.start();
  setImmediate(evaluate);
}
