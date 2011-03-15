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

// test creating and reading symbolic link
var linkData = path.join(common.fixturesDir, '/cycles/root.js');
var linkPath = path.join(common.tmpDir, 'symlink1.js');
fs.symlink(linkData, linkPath, function(err) {
  if (err) throw err;
  console.log('symlink done');
  // todo: fs.lstat?
  fs.readlink(linkPath, function(err, destination) {
    if (err) throw err;
    assert.equal(destination, linkData);
    completed++;
  });
});

// test creating and reading hard link
var srcPath = path.join(common.fixturesDir, 'cycles', 'root.js');
var dstPath = path.join(common.tmpDir, 'link1.js');
fs.link(srcPath, dstPath, function(err) {
  if (err) throw err;
  console.log('hard link done');
  var srcContent = fs.readFileSync(srcPath, 'utf8');
  var dstContent = fs.readFileSync(dstPath, 'utf8');
  assert.equal(srcContent, dstContent);
  completed++;
});

process.addListener('exit', function() {
  assert.equal(completed, 2);
});

