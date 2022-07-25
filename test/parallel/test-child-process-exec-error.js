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
const child_process = require('child_process');

function test(fn, code, expectPidType = 'number') {
  const child = fn('does-not-exist', common.mustCall(function(err) {
    assert.strictEqual(err.code, code);
    assert(err.cmd.includes('does-not-exist'));
  }));

  assert.strictEqual(typeof child.pid, expectPidType);
}

// With `shell: true`, expect pid (of the shell)
if (common.isWindows) {
  test(child_process.exec, 1, 'number'); // Exit code of cmd.exe
} else {
  test(child_process.exec, 127, 'number'); // Exit code of /bin/sh
}

// With `shell: false`, expect no pid
test(child_process.execFile, 'ENOENT', 'undefined');
