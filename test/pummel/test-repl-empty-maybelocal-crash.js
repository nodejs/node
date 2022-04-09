'use strict';
const common = require('../common');

if (process.config.variables.arm_version === '7') {
  common.skip('Too slow for armv7 bots');
}

// The process should not crash when the REPL receives the string, 'ss'.
// Test for https://github.com/nodejs/node/issues/42407.

const repl = require('repl');

const r = repl.start();

r.write('var buf = Buffer.from({length:200e6},(_,i) => i%256);\n');
r.write('var ss = buf.toString("binary");\n');
r.write('ss');
r.write('.');

r.close();
