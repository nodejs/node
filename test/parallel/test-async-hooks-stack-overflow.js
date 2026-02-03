'use strict';

// This test verifies that when a stack overflow occurs with async_hooks
// enabled, the uncaughtException handler is still called instead of the
// process crashing with exit code 7.

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

if (process.argv[2] === 'child') {
  const { createHook } = require('async_hooks');

  let handlerCalled = false;

  function recursive() {
    // Create a promise to trigger async_hooks init callback
    new Promise(() => {});
    return recursive();
  }

  createHook({ init() {} }).enable();

  process.on('uncaughtException', common.mustCall((err) => {
    assert.strictEqual(err.name, 'RangeError');
    assert.match(err.message, /Maximum call stack size exceeded/);
    // Ensure handler is only called once
    assert.strictEqual(handlerCalled, false);
    handlerCalled = true;
  }));

  setImmediate(recursive);
} else {
  // Parent process - spawn the child and check exit code
  const result = spawnSync(
    process.execPath,
    [__filename, 'child'],
    { encoding: 'utf8', timeout: 30000 }
  );

  // Should exit with code 0 (handler was called and handled the exception)
  // Previously would exit with code 7 (kExceptionInFatalExceptionHandler)
  assert.strictEqual(result.status, 0,
                     `Expected exit code 0, got ${result.status}.\n` +
                     `stdout: ${result.stdout}\n` +
                     `stderr: ${result.stderr}`);
}
