'use strict';
// Flags: --expose_internals

const common = require('../common');
const assert = require('assert');
const getValidStdio = require('internal/child_process').getValidStdio;

const expectedError = { code: 'ERR_INVALID_OPT_VALUE', type: TypeError };

// Should throw if string and not ignore, pipe, or inherit
common.expectsError(() => getValidStdio('foo'), expectedError);

// Should throw if not a string or array
common.expectsError(() => getValidStdio(600), expectedError);

// Should populate stdio with undefined if len < 3
{
  const stdio1 = [];
  const result = getValidStdio(stdio1, false);
  assert.strictEqual(stdio1.length, 3);
  assert.strictEqual(result.hasOwnProperty('stdio'), true);
  assert.strictEqual(result.hasOwnProperty('ipc'), true);
  assert.strictEqual(result.hasOwnProperty('ipcFd'), true);
}

// Should throw if stdio has ipc and sync is true
const stdio2 = ['ipc', 'ipc', 'ipc'];
common.expectsError(() => getValidStdio(stdio2, true),
                    { code: 'ERR_IPC_SYNC_FORK', type: Error }
);

// Should throw if stdio is not a valid input
{
  const stdio = ['foo'];
  common.expectsError(() => getValidStdio(stdio, false),
                      { code: 'ERR_INVALID_SYNC_FORK_INPUT', type: TypeError }
  );
}

// Should throw if stdio is not a valid option
{
  const stdio = [{ foo: 'bar' }];
  common.expectsError(() => getValidStdio(stdio), expectedError);
}

if (common.isMainThread) {
  const stdio3 = [process.stdin, process.stdout, process.stderr];
  const result = getValidStdio(stdio3, false);
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
