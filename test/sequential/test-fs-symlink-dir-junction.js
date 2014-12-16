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
var completed = 0;
var expected_tests = 4;

// test creating and reading symbolic link
var linkData = path.join(common.fixturesDir, 'cycles/');
var linkPath = path.join(common.tmpDir, 'cycles_link');

// Delete previously created link
try {
  fs.unlinkSync(linkPath);
} catch (e) {}

console.log('linkData: ' + linkData);
console.log('linkPath: ' + linkPath);

fs.symlink(linkData, linkPath, 'junction', function(err) {
  if (err) throw err;
  completed++;

  fs.lstat(linkPath, function(err, stats) {
    if (err) throw err;
    assert.ok(stats.isSymbolicLink());
    completed++;

    fs.readlink(linkPath, function(err, destination) {
      if (err) throw err;
      assert.equal(destination, linkData);
      completed++;

      fs.unlink(linkPath, function(err) {
        if (err) throw err;
        assert(!fs.existsSync(linkPath));
        assert(fs.existsSync(linkData));
        completed++;
      });
    });
  });
});

process.on('exit', function() {
  assert.equal(completed, expected_tests);
});

