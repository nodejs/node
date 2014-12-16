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
var path = require('path');
var fs = require('fs');
var got_error = false;
var success_count = 0;
var expected_count = 0;
var filename;

if (!process.promisifyCore) {
  console.log(
    '// Skipping tests of promisified API, `--promisify-core` is needed');
  return;
}

// happy path, all arguments
expected_count++;
fs.readFile(__filename)
  .then(function(content) {
    assert.ok(/happy path, all arguments/.test(content));
    success_count++;
  })
  .catch(reportTestError);

// empty file, optional arguments skipped
expected_count++;
filename = path.join(common.fixturesDir, 'empty.txt');
fs.readFile(filename, 'utf-8')
  .then(function(content) {
    assert.strictEqual('', content);
    success_count++;
  })
  .catch(reportTestError);

// file does not exist
expected_count++;
filename = path.join(common.fixturesDir, 'does_not_exist.txt');
fs.readFile(filename)
  .then(
    function(content) {
      throw new Error('readFile should have failed for unkown file');
    },
    function(err) {
      assert.equal('ENOENT', err.code);
      success_count++;
    })
  .catch(reportTestError);

process.on('exit', function() {
  assert.equal(expected_count, success_count);
  assert.equal(false, got_error);
});

function reportTestError(err) {
  console.error('\nTEST FAILED\n', err.stack, '\n');
  got_error = true;
}
