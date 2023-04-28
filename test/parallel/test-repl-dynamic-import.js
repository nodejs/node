'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');
const child = child_process.spawn(process.execPath, [
  '--interactive',
  '--expose-gc',
], {
  stdio: 'pipe'
});
child.stdin.write('\nimport("fs");\n_.then(gc);\n');
// Wait for concurrent GC to finish
setTimeout(() => {
  child.stdin.write('\nimport("fs");\n');
  child.stdin.write('\nprocess.exit(0);\n');
}, common.platformTimeout(50));
child.on('exit', (code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
});
