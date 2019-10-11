'use strict';
require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

const child = spawn(process.execPath, [
  '--interactive'
]);
child.stdin.end(`
import('fs').then(
  ns => ns.default === require('fs') ? 0 : 1,
  _ => 2
).then(process.exit)
`);

child.on('exit', (code) => {
  assert.strictEqual(code, 0);
});
