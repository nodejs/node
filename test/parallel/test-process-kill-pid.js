// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');

// Test variants of pid
//
// null: TypeError
// undefined: TypeError
//
// 'SIGTERM': TypeError
//
// String(process.pid): TypeError
//
// Nan, Infinity, -Infinity: TypeError
//
// 0, String(0): our group process
//
// process.pid, String(process.pid): ourself

['SIGTERM', null, undefined, NaN, Infinity, -Infinity].forEach((val) => {
  assert.throws(() => process.kill(val), {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "pid" argument must be of type number.' +
             common.invalidArgTypeHelper(val)
  });
});

// Test that kill throws an error for unknown signal names
assert.throws(() => process.kill(0, 'test'), {
  code: 'ERR_UNKNOWN_SIGNAL',
  name: 'TypeError',
  message: 'Unknown signal: test'
});

// Test that kill throws an error for invalid signal numbers
assert.throws(() => process.kill(0, 987), {
  code: 'EINVAL',
  name: 'Error',
  message: 'kill EINVAL'
});

// Test kill argument processing in valid cases.
//
// Monkey patch _kill so that we don't actually send any signals, particularly
// that we don't kill our process group, or try to actually send ANY signals on
// windows, which doesn't support them.
function kill(tryPid, trySig, expectPid, expectSig) {
  let getPid;
  let getSig;
  const origKill = process._kill;
  process._kill = function(pid, sig) {
    getPid = pid;
    getSig = sig;

    // un-monkey patch process._kill
    process._kill = origKill;
  };

  process.kill(tryPid, trySig);

  assert.strictEqual(getPid.toString(), expectPid.toString());
  assert.strictEqual(getSig, expectSig);
}

// Note that SIGHUP and SIGTERM map to 1 and 15 respectively, even on Windows
// (for Windows, libuv maps 1 and 15 to the correct behavior).

kill(0, 'SIGHUP', 0, 1);
kill(0, undefined, 0, 15);
kill('0', 'SIGHUP', 0, 1);
kill('0', undefined, 0, 15);

// Confirm that numeric signal arguments are supported

kill(0, 1, 0, 1);
kill(0, 15, 0, 15);

// Negative numbers are meaningful on unix
kill(-1, 'SIGHUP', -1, 1);
kill(-1, undefined, -1, 15);
kill('-1', 'SIGHUP', -1, 1);
kill('-1', undefined, -1, 15);

kill(process.pid, 'SIGHUP', process.pid, 1);
kill(process.pid, undefined, process.pid, 15);
kill(String(process.pid), 'SIGHUP', process.pid, 1);
kill(String(process.pid), undefined, process.pid, 15);
