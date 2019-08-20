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

/*
  Spawns 'pwd' with given options, then test
  - whether the exit code equals expectCode,
  - optionally whether the trimmed stdout result matches expectData
*/
function testCwd(options, expectCode = 0, expectData) {
  const child = spawn(...common.pwdCommand, options);

  child.stdout.setEncoding('utf8');

  // No need to assert callback since `data` is asserted.
  let data = '';
  child.stdout.on('data', function(chunk) {
    data += chunk;
  });

  // Can't assert callback, as stayed in to API:
  // _The 'exit' event may or may not fire after an error has occurred._
  child.on('exit', function(code, signal) {
    assert.strictEqual(code, expectCode);
  });

  child.on('close', common.mustCall(function() {
    expectData && assert.strictEqual(data.trim(), expectData);
  }));

  return child;
}


// Assume does-not-exist doesn't exist, expect exitCode=-1 and errno=ENOENT
{
  testCwd({ cwd: 'does-not-exist' }, -1)
    .on('error', common.mustCall(function(e) {
      assert.strictEqual(e.code, 'ENOENT');
    }));
}

// Assume these exist, and 'pwd' gives us the right directory back
testCwd({ cwd: common.rootDir }, 0, common.rootDir);
const shouldExistDir = common.isWindows ? process.env.windir : '/dev';
testCwd({ cwd: shouldExistDir }, 0, shouldExistDir);

// Spawn() shouldn't try to chdir() to invalid arg, so this should just work
testCwd({ cwd: '' });
testCwd({ cwd: undefined });
testCwd({ cwd: null });
