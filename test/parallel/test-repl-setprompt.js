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
const os = require('os');

const args = [
  '-e',
  'var e = new (require("repl")).REPLServer("foo.. "); e.context.e = e;',
];

const p = 'bar.. ';

const child = spawn(process.execPath, args);

child.stdout.setEncoding('utf8');

let data = '';
child.stdout.on('data', function(d) { data += d; });

child.stdin.end(`e.setPrompt("${p}");${os.EOL}`);

child.on('close', function(code, signal) {
  assert.strictEqual(code, 0);
  assert.ok(!signal);
  const lines = data.split('\n');
  assert.strictEqual(lines.pop(), p);
});
