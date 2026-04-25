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

// Regression test for https://github.com/nodejs/node/issues/25131.
//
// Since #18701 (v8.12) the parent's `child.stdin` socket no longer emitted a
// 'close' event when the child closed its end of the stdin pipe. The
// 'close' event should be observable so consumers can detect that the
// writable side of the pipe is gone without having to attempt a write and
// wait for EPIPE. This test exercises both:
//   1. The well-behaved case where the child simply exits without anyone
//      touching stdin — `child.stdin` must still emit 'close'.
//   2. The exact scenario from #25131 where a long-running child closes
//      its own fd 0 — the parent should observe 'close' eagerly, before
//      the child process exits.

const common = require('../common');
const { spawn } = require('child_process');

// Case 1: child exits without anyone touching stdin. `child.stdin` must
// emit 'close'.
{
  const child = spawn(process.execPath, ['-e', 'setTimeout(() => {}, 50)']);
  child.stdin.on('close', common.mustCall());
  child.on('exit', common.mustCall());
}

// Case 2: child explicitly closes its own stdin file descriptor while
// still running. The parent should observe 'close' on `child.stdin`
// eagerly, without waiting for the child process to exit. Skipped on
// Windows because the named-pipe semantics there do not propagate the
// peer close until the child process exits.
if (!common.isWindows) {
  const child = spawn(
    process.execPath,
    ['-e', 'require("fs").closeSync(0); setTimeout(() => {}, 2000)'],
    { stdio: ['pipe', 'inherit', 'inherit'] },
  );
  // The 'close' must fire well before the child's 2s timer expires.
  // common.platformTimeout keeps this comfortable on slow CI machines
  // while still catching the regression (which would only fire 'close'
  // after the child finally exits at ~2000ms).
  const eagerWindow = setTimeout(() => {
    throw new Error('child.stdin close did not fire eagerly on peer-close');
  }, common.platformTimeout(1500));
  child.stdin.on('close', common.mustCall(() => {
    clearTimeout(eagerWindow);
    // Kill the still-running child so the test does not have to wait
    // for its 2s timer.
    child.kill();
  }));
  child.on('exit', common.mustCall());
}
