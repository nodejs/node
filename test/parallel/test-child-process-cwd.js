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

let returns = 0;

/*
  Spawns 'pwd' with given options, then test
  - whether the exit code equals forCode,
  - optionally whether the stdout result matches forData
    (after removing trailing whitespace)
*/
function testCwd(options, forCode, forData) {
  let data = '';

  const child = common.spawnPwd(options);

  child.stdout.setEncoding('utf8');

  child.stdout.on('data', function(chunk) {
    data += chunk;
  });

  child.on('exit', function(code, signal) {
    assert.strictEqual(forCode, code);
  });

  child.on('close', function() {
    forData && assert.strictEqual(forData, data.replace(/[\s\r\n]+$/, ''));
    returns--;
  });

  returns++;

  return child;
}

// Assume these exist, and 'pwd' gives us the right directory back
testCwd({cwd: common.rootDir}, 0, common.rootDir);
if (common.isWindows) {
  testCwd({cwd: process.env.windir}, 0, process.env.windir);
} else {
  testCwd({cwd: '/dev'}, 0, '/dev');
}

// Assume does-not-exist doesn't exist, expect exitCode=-1 and errno=ENOENT
{
  testCwd({cwd: 'does-not-exist'}, -1).on('error', common.mustCall(function(e) {
    assert.strictEqual(e.code, 'ENOENT');
  }));
}

// Spawn() shouldn't try to chdir() so this should just work
testCwd(undefined, 0);
testCwd({}, 0);
testCwd({cwd: ''}, 0);
testCwd({cwd: undefined}, 0);
testCwd({cwd: null}, 0);

// Check whether all tests actually returned
assert.notStrictEqual(returns, 0);
process.on('exit', function() {
  assert.strictEqual(returns, 0);
});
