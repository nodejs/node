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

var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;
var path = require('path');

var returns = 0;

/*
  Spawns 'pwd' with given options, then test
  - whether the exit code equals forCode,
  - optionally whether the stdout result matches forData
    (after removing traling whitespace)
*/
function testCwd(options, forCode, forData) {
  var data = '';

  var child = spawn('pwd', [], options);
  child.stdout.setEncoding('utf8');

  child.stdout.addListener('data', function(chunk) {
    data += chunk;
  });

  child.addListener('exit', function(code, signal) {
    forData && assert.strictEqual(forData, data.replace(/[\s\r\n]+$/, ''));
    assert.strictEqual(forCode, code);
    returns--;
  });

  returns++;
}

// Assume these exist, and 'pwd' gives us the right directory back
testCwd({cwd: '/dev'}, 0, '/dev');
testCwd({cwd: '/'}, 0, '/');

// Assume this doesn't exist, we expect exitcode=127
testCwd({cwd: 'does-not-exist'}, 127);

// Spawn() shouldn't try to chdir() so this should just work
testCwd(undefined, 0);
testCwd({}, 0);
testCwd({cwd: ''}, 0);
testCwd({cwd: undefined}, 0);
testCwd({cwd: null}, 0);

// Check whether all tests actually returned
assert.notEqual(0, returns);
process.addListener('exit', function() {
  assert.equal(0, returns);
});
