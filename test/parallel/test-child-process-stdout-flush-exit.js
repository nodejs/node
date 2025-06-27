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

// If child process output to console and exit
// The console.log statements here are part of the test.
// Note: This test verifies specific behavior that is *not* guaranteed
// by Node.js's API contract. See https://nodejs.org/api/process.html#processexitcode.
// We are still generally interested in knowing when this test breaks,
// since applications may rely on the implicit behavior of stdout having
// a buffer size up to which they can write data synchronously.
if (process.argv[2] === 'child') {
  console.log('hello');
  for (let i = 0; i < 100; i++) {
    console.log('filler');
  }
  console.log('goodbye');
  process.exit(0);
} else {
  // parent process
  const spawn = require('child_process').spawn;

  // spawn self as child
  const child = spawn(process.argv[0], [process.argv[1], 'child']);

  let stdout = '';

  child.stderr.on('data', common.mustNotCall());

  // Check if we receive both 'hello' at start and 'goodbye' at end
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', common.mustCallAtLeast((data) => {
    stdout += data;
  }));

  child.on('close', common.mustCall(() => {
    assert.strictEqual(stdout.slice(0, 6), 'hello\n');
    assert.strictEqual(stdout.slice(stdout.length - 8), 'goodbye\n');
  }));
}
