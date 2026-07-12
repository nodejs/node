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
const { spawn } = require('child_process');

// Test stdio piping.
{
  const child = spawn(...common.pwdCommand, { stdio: ['pipe'] });
  assert.notStrictEqual(child.stdout, null);
  assert.notStrictEqual(child.stderr, null);
}

// Test stdio ignoring.
{
  const child = spawn(...common.pwdCommand, { stdio: 'ignore' });
  assert.strictEqual(child.stdout, null);
  assert.strictEqual(child.stderr, null);
}

// Asset options invariance.
{
  const options = { stdio: 'ignore' };
  spawn(...common.pwdCommand, options);
  assert.deepStrictEqual(options, { stdio: 'ignore' });
}

// Test stdout buffering.
{
  let output = '';
  const child = spawn(...common.pwdCommand);

  child.stdout.setEncoding('utf8');
  child.stdout.on('data', function(s) {
    output += s;
  });

  child.on('exit', common.mustCall(function(code) {
    assert.strictEqual(code, 0);
  }));

  child.on('close', common.mustCall(function() {
    assert.strictEqual(output.length > 1, true);
    assert.strictEqual(output[output.length - 1], '\n');
  }));
}

// Assert only one IPC pipe allowed.
assert.throws(
  () => {
    spawn(
      ...common.pwdCommand,
      { stdio: ['pipe', 'pipe', 'pipe', 'ipc', 'ipc'] }
    );
  },
  { code: 'ERR_IPC_ONE_PIPE', name: 'Error' }
);
