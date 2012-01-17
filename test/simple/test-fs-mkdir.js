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

function unlink(pathname) {
  try {
    fs.rmdirSync(pathname);
  } catch (e) {
  }
}

(function() {
  var ncalls = 0;
  var pathname = common.tmpDir + '/test1';

  unlink(pathname);

  fs.mkdir(pathname, function(err) {
    assert.equal(err, null);
    assert.equal(path.existsSync(pathname), true);
    ncalls++;
  });

  process.on('exit', function() {
    unlink(pathname);
    assert.equal(ncalls, 1);
  });
})();

(function() {
  var ncalls = 0;
  var pathname = common.tmpDir + '/test2';

  unlink(pathname);

  fs.mkdir(pathname, 511 /*=0777*/, function(err) {
    assert.equal(err, null);
    assert.equal(path.existsSync(pathname), true);
    ncalls++;
  });

  process.on('exit', function() {
    unlink(pathname);
    assert.equal(ncalls, 1);
  });
})();

(function() {
  var pathname = common.tmpDir + '/test3';

  unlink(pathname);
  fs.mkdirSync(pathname);

  var exists = path.existsSync(pathname);
  unlink(pathname);

  assert.equal(exists, true);
})();

// Keep the event loop alive so the async mkdir() requests
// have a chance to run (since they don't ref the event loop).
process.nextTick(function() {});
