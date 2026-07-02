'use strict';
require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

function isOnlcrEnabled() {
  const { stdout, status } = spawnSync('stty', ['-a'], {
    encoding: 'utf8',
    stdio: ['inherit', 'pipe', 'inherit'],
  });

  assert.strictEqual(status, 0);
  return /(?:^|[\s;])onlcr(?:[\s;]|$)/.test(stdout);
}

process.stdin.setRawMode(true);
console.log(`raw=${isOnlcrEnabled()}`);
assert.strictEqual(process.stdin.isRaw, true);
assert.strictEqual(process.stdin.rawMode, 'raw');

process.stdin.setRawMode(false);
console.log(`normal=${process.stdin.isRaw}`);
assert.strictEqual(process.stdin.rawMode, false);
assert.throws(
  () => process.stdin.setRawMode('raw-vt'),
  {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError',
  });
assert.strictEqual(process.stdin.rawMode, false);

process.stdin.setRawMode('raw');
console.log(`raw-string=${isOnlcrEnabled()}`);
assert.strictEqual(process.stdin.isRaw, true);
assert.strictEqual(process.stdin.rawMode, 'raw');

process.stdin.setRawMode(false);
process.stdin.setRawMode('io');
console.log(`io=${isOnlcrEnabled()}`);
assert.strictEqual(process.stdin.isRaw, true);
assert.strictEqual(process.stdin.rawMode, 'io');

process.stdin.setRawMode(false);
