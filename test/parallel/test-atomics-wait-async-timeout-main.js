'use strict';

require('../common');

if (process.argv[2] === 'child') {
  const sab = new SharedArrayBuffer(16);
  const i32a = new Int32Array(sab);
  const result = Atomics.waitAsync(i32a, 0, 0, 1000);
  result.value.then((val) => console.log(val));
  return;
}

const { spawnSync } = require('child_process');
const assert = require('assert');

const child = spawnSync(process.execPath, [__filename, 'child']);
assert.strictEqual(child.stdout.toString().trim(), 'timed-out');
assert.strictEqual(child.stderr.toString().trim(), '');
assert.strictEqual(child.status, 0);
