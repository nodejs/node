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

const spawnSync = require('child_process').spawnSync;

// Echo does different things on Windows and Unix, but in both cases, it does
// more-or-less nothing if there are no parameters
const ret = spawnSync('sleep', ['0']);
assert.strictEqual(ret.status, 0, 'exit status should be zero');

// Error test when command does not exist
const ret_err = spawnSync('command_does_not_exist', ['bar']).error;

assert.strictEqual(ret_err.code, 'ENOENT');
assert.strictEqual(ret_err.errno, 'ENOENT');
assert.strictEqual(ret_err.syscall, 'spawnSync command_does_not_exist');
assert.strictEqual(ret_err.path, 'command_does_not_exist');
assert.deepStrictEqual(ret_err.spawnargs, ['bar']);

{
  // Test the cwd option
  const cwd = common.rootDir;
  const response = common.spawnSyncPwd({ cwd });

  assert.strictEqual(response.stdout.toString().trim(), cwd);
}

{
  // Test the encoding option
  const noEncoding = common.spawnSyncPwd();
  const bufferEncoding = common.spawnSyncPwd({ encoding: 'buffer' });
  const utf8Encoding = common.spawnSyncPwd({ encoding: 'utf8' });

  assert.deepStrictEqual(noEncoding.output, bufferEncoding.output);
  assert.deepStrictEqual([
    null,
    noEncoding.stdout.toString(),
    noEncoding.stderr.toString()
  ], utf8Encoding.output);
}
