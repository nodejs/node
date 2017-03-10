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

// if child process output to console and exit
if (process.argv[2] === 'child') {
  console.log('hello');
  for (let i = 0; i < 200; i++) {
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

  child.stderr.setEncoding('utf8');
  child.stderr.on('data', function(data) {
    common.fail(`Unexpected parent stderr: ${data}`);
  });

  // check if we receive both 'hello' at start and 'goodbye' at end
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', function(data) {
    stdout += data;
  });

  child.on('close', common.mustCall(function() {
    assert.strictEqual(stdout.slice(0, 6), 'hello\n');
    assert.strictEqual(stdout.slice(stdout.length - 8), 'goodbye\n');
  }));
}
