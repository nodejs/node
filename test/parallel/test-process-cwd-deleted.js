'use strict';

// This test verifies that an appropriate error is thrown when the current
// working directory is deleted and process.cwd() is called.
//
// We do this by spawning a forked process and deleting the tmp cwd
// while it is starting up.

const common = require('../common');
const { fork } = require('node:child_process');
const { rmSync } = require('node:fs');
const assert = require('node:assert');

if (process.argv[2] === 'child') {
  while (true) {
    process.cwd();  // Call process.cwd() to trigger failure
    process.chdir('.'); // Reset the cache to force re-evaluation
  }
}

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const tmpDir = tmpdir.path;

const proc = fork(__filename, ['child'], {
  cwd: tmpDir,
  silent: true,
});

let stderrData = '';

proc.stderr.on('data', (chunk) => {
  stderrData += chunk.toString(); // Collect stderr output
});

proc.on('spawn', common.mustCall(() => rmSync(tmpDir, { recursive: true })));

proc.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 1); // Ensure process exits with error
  assert.match(stderrData, /Current working directory does not exist/);
}));
