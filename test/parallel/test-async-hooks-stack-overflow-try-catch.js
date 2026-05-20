'use strict';

// This test verifies that when a stack overflow occurs with async_hooks
// enabled, the exception can be caught by try-catch blocks in user code.

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

if (process.argv[2] === 'child') {
  const { createHook } = require('async_hooks');

  createHook({ init() {} }).enable();

  function recursive(depth = 0) {
    // Create a promise to trigger async_hooks init callback
    new Promise(() => {});
    return recursive(depth + 1);
  }

  try {
    recursive();
    // Should not reach here
    process.exit(1);
  } catch (err) {
    assert.strictEqual(err.name, 'RangeError');
    assert.match(err.message, /Maximum call stack size exceeded/);
    console.log('SUCCESS: try-catch caught the stack overflow');
    process.exit(0);
  }

  // Should not reach here
  process.exit(2);
} else {
  // Parent process - spawn the child and check exit code
  const result = spawnSync(
    process.execPath,
    [__filename, 'child'],
    { encoding: 'utf8', timeout: 30000 }
  );

  assert.strictEqual(result.status, 0,
                     `Expected exit code 0 (try-catch worked), got ${result.status}.\n` +
                     `stdout: ${result.stdout}\n` +
                     `stderr: ${result.stderr}`);
  assert.match(result.stdout, /SUCCESS: try-catch caught the stack overflow/);
}
