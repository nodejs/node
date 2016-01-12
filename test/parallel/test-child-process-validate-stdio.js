'use strict';
// Flags: --expose_internals

require('../common');
const assert = require('assert');
const _validateStdio = require('internal/child_process')._validateStdio;

function checkerr(err) {
  return /^Incorrect value of stdio option: /.test(err.message);
}

function checkerr2(err) {
  return /^IPC cannot be used with synchronous fork/.test(err.message);
}

// should throw if string and not ignore, pipe, or inherit
assert.throws(() => _validateStdio('foo'), checkerr);

// should throw if not a string or array
assert.throws(() => _validateStdio(600), checkerr);

// should populate stdio with undefined if len < 3
var stdio1 = [];
var result = _validateStdio(stdio1, false);
assert.equal(stdio1.length, 3);
assert.equal(result.hasOwnProperty('stdio'), true);
assert.equal(result.hasOwnProperty('ipc'), true);
assert.equal(result.hasOwnProperty('ipcFd'), true);

// should throw if stdio has ipc and sync is true
var stdio2 = ['ipc', 'ipc', 'ipc'];
assert.throws(() => _validateStdio(stdio2, true), checkerr2);

const stdio3 = [process.stdin, process.stdout, process.stderr];
var result = _validateStdio(stdio3, false);
assert.deepStrictEqual(result, {
  stdio: [
    { type: 'fd', fd: 0 },
    { type: 'fd', fd: 1 },
    { type: 'fd', fd: 2 }
  ],
  ipc: undefined,
  ipcFd: undefined
});
