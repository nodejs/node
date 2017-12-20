'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

if (process.argv[2] === 'child') {
  process.abort();
} else {
  const child = cp.spawnSync(`${process.execPath}`, [`${__filename}`, 'child']);
  const stderr = child.stderr.toString();

  assert.strictEqual(child.stdout.toString(), '');
  // stderr will be empty for systems that don't support backtraces.
  if (stderr !== '') {
    const frames = stderr.trimRight().split('\n').map((s) => s.trim());

    if (!frames.every((frame, index) => frame.startsWith(`${index + 1}:`))) {
      assert.fail(`Each frame should start with a frame number:\n${stderr}`);
    }

    if (!common.isWindows) {
      if (!frames.some((frame) => frame.includes(`[${process.execPath}]`))) {
        assert.fail(`Some frames should include the binary name:\n${stderr}`);
      }
    }
  }
}
