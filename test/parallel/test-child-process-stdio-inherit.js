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
require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

if (process.argv[2] === 'parent')
  parent();
else
  grandparent();

function grandparent() {
  const child = spawn(process.execPath, [__filename, 'parent']);
  child.stderr.pipe(process.stderr);
  let output = '';
  const input = 'asdfasdf';

  child.stdout.on('data', function(chunk) {
    output += chunk;
  });
  child.stdout.setEncoding('utf8');

  child.stdin.end(input);

  child.on('close', function(code, signal) {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    // 'cat' on windows adds a \r\n at the end.
    assert.strictEqual(output.trim(), input.trim());
  });
}

function parent() {
  // Should not immediately exit.
  spawn('cat', [], { stdio: 'inherit' });
}
