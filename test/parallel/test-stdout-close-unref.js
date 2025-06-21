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
const spawn = require('child_process').spawn;

if (process.argv[2] === 'child') {
  process.stdin.resume();
  process.stdin._handle.close();
  process.stdin._handle.unref();  // Should not segfault.
  process.stdin.on('error', common.mustCall());
  return;
}

// Use spawn so that we can be sure that stdin has a _handle property.
// Refs: https://github.com/nodejs/node/pull/5916
const proc = spawn(process.execPath, [__filename, 'child'], { stdio: 'pipe' });

proc.stderr.pipe(process.stderr);
proc.on('exit', common.mustCall(function(exitCode) {
  if (exitCode !== 0)
    process.exitCode = exitCode;
}));
