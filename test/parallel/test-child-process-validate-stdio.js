'use strict';
// Flags: --expose-internals

const common = require('../common');
const assert = require('assert');
const { createPipe } = require('node:net');
const { PassThrough } = require('stream');
const getValidStdio = require('internal/child_process').getValidStdio;

const expectedError = { code: 'ERR_INVALID_ARG_VALUE', name: 'TypeError' };

// Should throw if string and not ignore, pipe, or inherit
assert.throws(() => getValidStdio('foo'), expectedError);

// Should throw if not a string or array
assert.throws(() => getValidStdio(600), expectedError);

// Should populate stdio with undefined if len < 3
{
  const stdio1 = [];
  const result = getValidStdio(stdio1, false);
  assert.strictEqual(stdio1.length, 3);
  assert.strictEqual(Object.hasOwn(result, 'stdio'), true);
  assert.strictEqual(Object.hasOwn(result, 'ipc'), true);
  assert.strictEqual(Object.hasOwn(result, 'ipcFd'), true);
}

// Should throw if stdio has ipc and sync is true
const stdio2 = ['ipc', 'ipc', 'ipc'];
assert.throws(() => getValidStdio(stdio2, true),
              { code: 'ERR_IPC_SYNC_FORK', name: 'Error' }
);

// Should throw if stdio is not a valid input
{
  const stdio = ['foo'];
  assert.throws(() => getValidStdio(stdio, false),
                { code: 'ERR_INVALID_SYNC_FORK_INPUT', name: 'TypeError' }
  );
}

// Should throw if stdio is not a valid option
{
  const stdio = [{ foo: 'bar' }];
  assert.throws(() => getValidStdio(stdio), expectedError);
}

// Pure JavaScript streams do not have an OS handle that can be passed to a
// child process as stdio.
{
  const stdio = [new PassThrough()];
  assert.throws(() => getValidStdio(stdio), expectedError);
}

// Parent-owned endpoint streams are normalized to their own stdio type because
// they lend only the OS handle to the child.
{
  const { readable, writable } = createPipe();
  const result = getValidStdio([readable, writable, 'ignore']);

  assert.strictEqual(result.stdio[0].type, 'leased');
  assert.strictEqual(result.stdio[0].stream, readable);
  assert.strictEqual(result.stdio[0].handle, readable._handle);
  assert.strictEqual(result.stdio[1].type, 'leased');
  assert.strictEqual(result.stdio[1].stream, writable);
  assert.strictEqual(result.stdio[1].handle, writable._handle);

  readable.destroy();
  writable.destroy();
}

// Parent-owned endpoint streams cannot be used with spawnSync() because the sync
// process runner only owns stdio pipes it creates internally.
{
  const { readable, writable } = createPipe();

  assert.throws(() => getValidStdio([readable, 'ignore', 'ignore'], true), {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /parent-owned endpoint streams are only supported by spawn\(\)/,
  });

  readable.destroy();
  writable.destroy();
}

const { isMainThread } = require('worker_threads');

if (isMainThread) {
  const stdio3 = [process.stdin, process.stdout, process.stderr];
  const result = getValidStdio(stdio3, false);
  assert.deepStrictEqual(result, {
    stdio: [
      { type: 'fd', fd: 0 },
      { type: 'fd', fd: 1 },
      { type: 'fd', fd: 2 },
    ],
    ipc: undefined,
    ipcFd: undefined
  });
} else {
  common.printSkipMessage(
    'stdio is not associated with file descriptors in Workers');
}
