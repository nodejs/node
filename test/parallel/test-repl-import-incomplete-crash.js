'use strict';
const common = require('../common');
const child_process = require('child_process');
const assert = require('assert');

// Regression test for https://github.com/nodejs/node/issues/63551:
// Typing a bare `import` keyword in the REPL must not crash the process.
const proc = child_process.spawn(process.execPath, ['-i']);
proc.on('error', common.mustNotCall());
proc.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 0);
}));
proc.stdin.write('import\n.exit\n');
