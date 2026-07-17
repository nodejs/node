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
const { isWindows } = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;
const debug = require('util').debuglog('test');

let success_count = 0;
let error_count = 0;
let response = '';
let child;

function after(err, stdout, stderr) {
  if (err) {
    error_count++;
    debug(`error!: ${err.code}`);
    debug(`stdout: ${JSON.stringify(stdout)}`);
    debug(`stderr: ${JSON.stringify(stderr)}`);
    assert.strictEqual(err.killed, false);
  } else {
    success_count++;
    assert.notStrictEqual(stdout, '');
  }
}

if (!isWindows) {
  child = exec('/usr/bin/env', { env: { 'HELLO': 'WORLD' } }, after);
} else {
  child = exec('set',
               { env: { ...process.env, 'HELLO': 'WORLD' } },
               after);
}

child.stdout.setEncoding('utf8');
child.stdout.on('data', function(chunk) {
  response += chunk;
});

process.on('exit', function() {
  debug('response: ', response);
  assert.strictEqual(success_count, 1);
  assert.strictEqual(error_count, 0);
  assert.ok(response.includes('HELLO=WORLD'));
});
