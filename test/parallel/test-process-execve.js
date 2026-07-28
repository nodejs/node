'use strict';

const { isWindows, isIBMi, skip } = require('../common');
const assert = require('assert');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  skip('process.execve is not available in Workers');
} else if (isWindows || isIBMi) {
  skip('process.execve is not available in Windows or IBM i');
}

if (process.argv[2] === 'replaced') {
  assert.deepStrictEqual(process.argv, [process.execPath, __filename, 'replaced']);
  assert.strictEqual(process.env.CWD, process.cwd());
  assert.strictEqual(process.env.EXECVE_A, 'FIRST');
  assert.strictEqual(process.env.EXECVE_B, 'SECOND');
} else {
  process.execve(
    process.execPath,
    [process.execPath, __filename, 'replaced'],
    { ...process.env, EXECVE_A: 'FIRST', EXECVE_B: 'SECOND', CWD: process.cwd() }
  );

  // If process.execve succeed, this should never be executed.
  assert.fail('process.execve failed');
}
