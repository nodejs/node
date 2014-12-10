// Copyright io.js contributors.
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
var fs = require('fs');
var path = require('path');
var doesNotExist = __filename + '__this_should_not_exist';
var readOnlyFile = path.join(common.tmpDir, 'read_only_file');

var removeFile = function(file) {
  try {
    fs.unlinkSync(file);
  } catch (err) {
    // Ignore error
  }
};

var createReadOnlyFile = function(file) {
  removeFile(file);
  fs.writeFileSync(file, '');
  fs.chmodSync(file, 0444);
};

createReadOnlyFile(readOnlyFile);

assert(typeof fs.F_OK === 'number');
assert(typeof fs.R_OK === 'number');
assert(typeof fs.W_OK === 'number');
assert(typeof fs.X_OK === 'number');

fs.access(__filename, function(err) {
  assert.strictEqual(err, null, 'error should not exist');
});

fs.access(__filename, fs.R_OK, function(err) {
  assert.strictEqual(err, null, 'error should not exist');
});

fs.access(doesNotExist, function(err) {
  assert.notEqual(err, null, 'error should exist');
  assert.strictEqual(err.code, 'ENOENT');
  assert.strictEqual(err.path, doesNotExist);
});

fs.access(readOnlyFile, fs.F_OK | fs.R_OK, function(err) {
  assert.strictEqual(err, null, 'error should not exist');
});

fs.access(readOnlyFile, fs.W_OK, function(err) {
  assert.notEqual(err, null, 'error should exist');
  assert.strictEqual(err.path, readOnlyFile);
});

assert.throws(function() {
  fs.access(100, fs.F_OK, function(err) {});
}, /path must be a string/);

assert.throws(function() {
  fs.access(__filename, fs.F_OK);
}, /callback must be a function/);

assert.doesNotThrow(function() {
  fs.accessSync(__filename);
});

assert.doesNotThrow(function() {
  var mode = fs.F_OK | fs.R_OK | fs.W_OK;

  fs.accessSync(__filename, mode);
});

assert.throws(function() {
  fs.accessSync(doesNotExist);
}, function (err) {
  return err.code === 'ENOENT' && err.path === doesNotExist;
});

process.on('exit', function() {
  removeFile(readOnlyFile);
});
