'use strict';
const common = require('../common');

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

require('child_process')
  .fork(__filename, ['child'], { timeout: common.platformTimeout(15000) })
  .on('exit', (exitCode) => process.exit(exitCode === null ? 1 : exitCode));
