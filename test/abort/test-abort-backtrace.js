'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

if (common.isWindows) {
  common.skip('Backtraces unimplemented on Windows.');
  return;
}

if (process.argv[2] === 'child') {
  process.abort();
} else {
  const child = cp.spawnSync(`${process.execPath}`, [`${__filename}`, 'child']);
  const frames = child.stderr.toString().trimRight().split('\n')
                   .map((s) => { return s.trim(); });

  assert.strictEqual(child.stdout.toString(), '');
  assert.ok(frames.length > 0);
  // All frames should start with a frame number.
  assert.ok(
    frames.every(
      (frame, index) => { return frame.startsWith(`${index + 1}:`); }
    )
  );
  // At least some of the frames should include the binary name.
  assert.ok(
    frames.some(
      (frame) => { return frame.includes(`[${process.execPath}]`); }
    )
  );
}
