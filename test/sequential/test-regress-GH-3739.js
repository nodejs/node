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

var common = require('../common.js'),
    assert = require('assert'),
    fs = require('fs'),
    path = require('path');

var dir = path.resolve(common.fixturesDir),
    dirs = [];

// Make a long path.
for (var i = 0; i < 50; i++) {
  dir = dir + '/123456790';
  try {
    fs.mkdirSync(dir, '0777');
  } catch (e) {
    if (e.code == 'EEXIST') {
      // Ignore;
    } else {
      cleanup();
      throw e;
    }
  }
  dirs.push(dir);
}

// Test existsSync
var r = fs.existsSync(dir);
if (r !== true) {
  cleanup();
  throw new Error('fs.existsSync returned false');
}

// Text exists
fs.exists(dir, function(r) {
  cleanup();
  if (r !== true) {
    throw new Error('fs.exists reported false');
  }
});

// Remove all created directories
function cleanup() {
  for (var i = dirs.length - 1; i >= 0; i--) {
    fs.rmdirSync(dirs[i]);
  }
}
