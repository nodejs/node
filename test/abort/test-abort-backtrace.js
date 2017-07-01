'use strict';
const common = require('../common');
if (common.isWindows)
  common.skip('Backtraces unimplemented on Windows.');

const assert = require('assert');
const cp = require('child_process');

if (process.argv[2] === 'child') {
  process.abort();
} else {
  const child = cp.spawnSync(`${process.execPath}`, [`${__filename}`, 'child']);
  const frames =
      child.stderr.toString().trimRight().split('\n').map((s) => s.trim());

  assert.strictEqual(child.stdout.toString(), '');
  assert.ok(frames.length > 0);
  // All frames should start with a frame number.
  assert.ok(frames.every((frame, index) => frame.startsWith(`${index + 1}:`)));
  // At least some of the frames should include the binary name.
  assert.ok(frames.some((frame) => frame.includes(`[${process.execPath}]`)));
}
