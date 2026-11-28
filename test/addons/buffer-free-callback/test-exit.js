'use strict';

// The backing store of an external Buffer may be freed by V8 on another thread
// while the environment is being cleaned up. That must not wake an async
// handle that is already closing, which aborts the process on Windows.

const common = require('../../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

if (process.argv[2] === 'child') {
  const binding = require(`./build/${common.buildType}/binding`);
  for (let i = 0; i < 20000; i++) binding.alloc(64, 1, 0);
  // Start freeing them and exit right away.
  globalThis.gc({ type: 'minor' });
  return;
}

// The race is not hit on every run.
for (let i = 0; i < 20; i++) {
  const child = spawnSync(
    process.execPath, ['--expose-gc', __filename, 'child'], { encoding: 'utf8' });
  assert.strictEqual(child.stderr, '');
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.status, 0);
}
