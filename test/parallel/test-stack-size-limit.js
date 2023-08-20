'use strict';

require('../common');

const assert = require('assert');
const { spawnSync } = require('child_process');

// The default --stack-size is 984, which is below Windows' default stack size
// limit of 1 MiB. However, even a slight increase would cause node to exceed
// the 1 MiB limit and thus to crash with the exit code STATUS_STACK_OVERFLOW.
// Newer versions of Node.js allow the stack size to grow to up to 8 MiB, which
// better aligns with default limits on other platforms and which is commonly
// used for browsers on Windows.
// See https://github.com/nodejs/node/issues/43630.

const { status, signal, stderr } = spawnSync(process.execPath, [
  '--stack-size=2000',
  '-e',
  '(function explode() { return explode(); })()',
], {
  encoding: 'utf8'
});

assert.strictEqual(status, 1);
assert.strictEqual(signal, null);
assert.match(stderr, /Maximum call stack size exceeded/);
