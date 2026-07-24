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
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const assert = require('assert');
const { spawnSync } = require('child_process');
const { getSystemErrorName } = require('util');

// `sleep` does different things on Windows and Unix, but in both cases, it does
// more-or-less nothing if there are no parameters
const ret = spawnSync('sleep', ['0']);
assert.strictEqual(ret.status, 0);

// Error test when command does not exist
const ret_err = spawnSync('command_does_not_exist', ['bar']).error;

assert.strictEqual(ret_err.code, 'ENOENT');
assert.strictEqual(getSystemErrorName(ret_err.errno), 'ENOENT');
assert.strictEqual(ret_err.syscall, 'spawnSync command_does_not_exist');
assert.strictEqual(ret_err.path, 'command_does_not_exist');
assert.deepStrictEqual(ret_err.spawnargs, ['bar']);

{
  // Test the cwd option
  const cwd = tmpdir.path;
  const response = spawnSync(...common.pwdCommand, { cwd });

  assert.strictEqual(response.stdout.toString().trim(), cwd);
}


{
  // Assert Buffer is the default encoding
  const retDefault = spawnSync(...common.pwdCommand);
  const retBuffer = spawnSync(...common.pwdCommand, { encoding: 'buffer' });
  assert.deepStrictEqual(retDefault.output, retBuffer.output);

  const retUTF8 = spawnSync(...common.pwdCommand, { encoding: 'utf8' });
  const stringifiedDefault = [
    null,
    retDefault.stdout.toString(),
    retDefault.stderr.toString(),
  ];
  assert.deepStrictEqual(retUTF8.output, stringifiedDefault);
}

{
  // Verify support for stdio.wrap via using another child process stream.
  // This is the same logic as in ch_sy.js, expressed as a deterministic test.
  if (common.isWindows) {
    common.skip('Not applicable on Windows');
  }

  const { spawn } = require('child_process');
  const childA = spawn(
    process.execPath,
    ['-e', 'process.stdin.pipe(process.stdout);'],
    { stdio: ['pipe', 'pipe', 'ignore'] }
  );

  let collected = '';
  childA.stdout.setEncoding('utf8');
  childA.stdout.on('data', (chunk) => { collected += chunk; });

  childA.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(collected, 'hi');
  }));

  const result = spawnSync(
    process.execPath,
    ['-e', 'process.stdout.write("hi")'],
    { stdio: ['inherit', childA.stdin, 'inherit'] }
  );

  assert.strictEqual(result.status, 0);
  // Explicitly close the wrapped stream on the parent side so childA receives EOF.
  childA.stdin.end();
}
