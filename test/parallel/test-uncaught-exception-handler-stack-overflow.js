'use strict';

// This test verifies that when the uncaughtException handler itself causes
// a stack overflow, the process exits with a non-zero exit code.
// This is important to ensure we don't silently swallow errors.

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

if (process.argv[2] === 'child') {
  function f() { f(); }
  process.on('uncaughtException', f);
  throw new Error('X');
} else {
  // Parent process - spawn the child and check exit code
  const result = spawnSync(
    process.execPath,
    [__filename, 'child'],
    { encoding: 'utf8', timeout: 30000 }
  );

  // Should exit with non-zero exit code since the uncaughtException handler
  // itself caused a stack overflow.
  assert.notStrictEqual(result.status, 0,
                        `Expected non-zero exit code, got ${result.status}.\n` +
                        `stdout: ${result.stdout}\n` +
                        `stderr: ${result.stderr}`);
}
