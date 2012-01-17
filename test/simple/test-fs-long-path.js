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
var fs = require('fs');
var path = require('path');
var assert = require('assert');

var successes = 0;

// make a path that will be at least 260 chars long.
var fileNameLen = Math.max(260 - common.tmpDir.length - 1, 1);
var fileName = path.join(common.tmpDir, new Array(fileNameLen + 1).join('x'));
var fullPath = path.resolve(fileName);

console.log({
  filenameLength: fileName.length,
  fullPathLength: fullPath.length
});

fs.writeFile(fullPath, 'ok', function(err) {
  if (err) throw err;
  successes++;

  fs.stat(fullPath, function(err, stats) {
    if (err) throw err;
    successes++;
  });
});

process.on('exit', function() {
  assert.equal(2, successes);
});
