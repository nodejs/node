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
var exec = require('child_process').exec;
var path = require('path');

var callbacks = 0;

function test(env, cb) {
  var filename = path.join(common.fixturesDir, 'test-fs-readfile-error.js');
  var execPath = process.execPath + ' --throw-deprecation ' + filename;
  var options = { env: env || {} };
  exec(execPath, options, function(err, stdout, stderr) {
    assert(err);
    assert.equal(stdout, '');
    assert.notEqual(stderr, '');
    cb('' + stderr);
  });
}

test({ NODE_DEBUG: '' }, function(data) {
  assert(/EISDIR/.test(data));
  assert(!/test-fs-readfile-error/.test(data));
  callbacks++;
});

test({ NODE_DEBUG: 'fs' }, function(data) {
  assert(/EISDIR/.test(data));
  assert(/test-fs-readfile-error/.test(data));
  callbacks++;
});

process.on('exit', function() {
  assert.equal(callbacks, 2);
});

(function() {
  console.error('the warnings are normal here.');
  // just make sure that this doesn't crash the process.
  var fs = require('fs');
  fs.readFile(__dirname);
  fs.readdir(__filename);
  fs.unlink('gee-i-sure-hope-this-file-isnt-important-or-existing');
})();
