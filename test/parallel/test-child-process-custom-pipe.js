'use strict';

const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

// Test that custom stdio pipes have correct readable/writable properties
// Regression test for pre-existing fork-stdio bug where custom pipes (stdio[3+])
// were incorrectly marked as readable instead of writable.

if (process.argv[2] === 'child') {
  // Child process just exits
  process.exit(0);
} else {
  // Parent process: verify socket properties
  const child = cp.fork(__filename, ['child'], {
    stdio: [0, 'ignore', 'pipe', 'ipc', 'pipe']
  });

  // stdio[0] = inherit (stdin)
  // stdio[1] = ignore 
  // stdio[2] = pipe (stderr)
  // stdio[3] = ipc
  // stdio[4] = pipe (custom pipe)

  const customPipe = child.stdio[4];

  // Custom pipe should be writable (parent writes data to child)
  assert.strictEqual(customPipe.writable, true,
    'Custom stdio pipe should be writable');

  // Custom pipe should not be readable by default
  assert.strictEqual(customPipe.readable, false,
    'Custom stdio pipe should not be readable by default');

  child.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
}
