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
var successes = 0;

var file = path.join(common.fixturesDir, 'a.js');

common.error('open ' + file);

fs.open(file, 'a', 0777, function(err, fd) {
  common.error('fd ' + fd);
  if (err) throw err;

  fs.fdatasyncSync(fd);
  common.error('fdatasync SYNC: ok');
  successes++;

  fs.fsyncSync(fd);
  common.error('fsync SYNC: ok');
  successes++;

  fs.fdatasync(fd, function(err) {
    if (err) throw err;
    common.error('fdatasync ASYNC: ok');
    successes++;
    fs.fsync(fd, function(err) {
      if (err) throw err;
      common.error('fsync ASYNC: ok');
      successes++;
    });
  });
});

process.addListener('exit', function() {
  assert.equal(4, successes);
});
