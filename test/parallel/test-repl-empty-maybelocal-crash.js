'use strict';
require('../common');

// The process should not crash when the REPL receives the string, 'ss'.
// Test for https://github.com/nodejs/node/issues/42407.

if (process.argv[2] === 'child') {
  const repl = require('repl');

  const r = repl.start();

  r.write('var buf = Buffer.from({length:200e6},(_,i) => i%256);\n');
  r.write('var ss = buf.toString("binary");\n');
  r.write('ss');
  r.write('.');

  r.close();
  return;
}

const controller = new AbortController();
const { signal } = controller;
setTimeout(() => controller.abort(), 20_000);
require('child_process').fork(__filename, ['child'], { signal })
  .on('exit', (code) => process.exit(code === null ? 1 : code))
  .on('error', (err) => {
    if (err.name === 'AbortError') process.exit();
    throw err;
  });
