'use strict';
const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

const child = spawn(process.execPath, [
  '--interactive',
]);
child.stdin.end(`
import('fs').then(
  ns => ns.default === require('fs') ? 0 : 1,
  _ => 2
).then(process.exit)
`);

child.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 0);
}));
