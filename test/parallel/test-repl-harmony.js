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
const args = ['-i'];
const child = spawn(process.execPath, args);

const input = '(function(){"use strict"; const y=1;y=2})()\n';
// This message will vary based on JavaScript engine, so don't check the message
// contents beyond confirming that the `Error` is a `TypeError`.
const expectOut = /> Uncaught TypeError: /;

child.stderr.setEncoding('utf8');
child.stderr.on('data', (d) => {
  throw new Error('child.stderr be silent');
});

child.stdout.setEncoding('utf8');
let out = '';
child.stdout.on('data', (d) => {
  out += d;
});
child.stdout.on('end', () => {
  assert.match(out, expectOut);
  console.log('ok');
});

child.stdin.end(input);
