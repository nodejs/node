'use strict';
// Flags: --expose_internals

const common = require('../common');
const assert = require('assert');
const _validateStdio = require('internal/child_process')._validateStdio;

const expectedError =
  common.expectsError({ code: 'ERR_INVALID_OPT_VALUE', type: TypeError }, 2);

// Should throw if string and not ignore, pipe, or inherit
assert.throws(() => _validateStdio('foo'), expectedError);

// should throw if not a string or array
assert.throws(() => _validateStdio(600), expectedError);

// should populate stdio with undefined if len < 3
{
  const stdio1 = [];
  const result = _validateStdio(stdio1, false);
  assert.strictEqual(stdio1.length, 3);
  assert.strictEqual(result.hasOwnProperty('stdio'), true);
  assert.strictEqual(result.hasOwnProperty('ipc'), true);
  assert.strictEqual(result.hasOwnProperty('ipcFd'), true);
}

// should throw if stdio has ipc and sync is true
const stdio2 = ['ipc', 'ipc', 'ipc'];
common.expectsError(() => _validateStdio(stdio2, true),
                    { code: 'ERR_IPC_SYNC_FORK', type: Error }
);


if (common.isMainThread) {
  const stdio3 = [process.stdin, process.stdout, process.stderr];
  const result = _validateStdio(stdio3, false);
  assert.deepStrictEqual(result, {
    stdio: [
      { type: 'fd', fd: 0 },
      { type: 'fd', fd: 1 },
      { type: 'fd', fd: 2 }
    ],
    ipc: undefined,
    ipcFd: undefined
  });
} else {
  common.printSkipMessage(
    'stdio is not associated with file descriptors in Workers');
}
