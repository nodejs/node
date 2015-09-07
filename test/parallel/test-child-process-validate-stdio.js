'use strict';
// Flags: --expose_internals

var assert = require('assert');
var common = require('../common');
var _validateStdio = require('internal/child_process')._validateStdio;

// should throw if string and not ignore, pipe, or inherit
assert.throws(function() {
  _validateStdio('foo');
}, /Incorrect value of stdio option/);

// should throw if not a string or array
assert.throws(function() {
  _validateStdio(600);
}, /Incorrect value of stdio option/);

// should populate stdio with undefined if len < 3
var stdio1 = [];
var result = _validateStdio(stdio1, false);
assert.equal(stdio1.length, 3);
assert.equal(result.hasOwnProperty('stdio'), true);
assert.equal(result.hasOwnProperty('ipc'), true);
assert.equal(result.hasOwnProperty('ipcFd'), true);

// should throw if stdio has ipc and sync is true
var stdio2 = ['ipc', 'ipc', 'ipc'];
assert.throws(function() {
  _validateStdio(stdio2, true);
}, /You cannot use IPC with synchronous forks/);

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
