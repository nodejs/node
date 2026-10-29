'use strict';

const common = require('../common.js');
const repl = require('node:repl');
const { PassThrough, Writable } = require('node:stream');

const bench = common.createBenchmark(main, {
  n: [1e4],
  code: [
    '1 + 1\n',
    'Promise.resolve(42)\n',
  ],
  mode: ['sloppy', 'strict'],
  terminal: [0, 1],
  useGlobal: [0, 1],
});

function main({ n, code: inputCode, mode, terminal, useGlobal }) {
  const input = new PassThrough();
  const output = new Writable({ write(c, e, cb) { cb(); } });
  const server = new repl.REPLServer({
    input,
    output,
    replMode: mode === 'strict' ?
      repl.REPL_MODE_STRICT :
      repl.REPL_MODE_SLOPPY,
    terminal: !!terminal,
    useGlobal: !!useGlobal,
  });
  const originalEval = server.eval;
  // TTY input dispatch can briefly have no other active event loop handles.
  const keepAlive = setInterval(() => {}, 0x7fffffff);
  let remaining = n;

  // eslint-disable-next-line node-core/func-name-matching
  server.eval = function REPLEval(code, context, file, callback) {
    originalEval(code, context, file, function onEvaluate() {
      const result = Reflect.apply(callback, this, arguments);
      if (--remaining === 0) {
        bench.end(n);
        clearInterval(keepAlive);
        server.close();
      } else {
        setImmediate(() => input.write(inputCode));
      }
      return result;
    });
  };

  bench.start();
  input.write(inputCode);
}
