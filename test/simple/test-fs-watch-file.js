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

// libuv-broken


var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');


var filename = path.join(common.fixturesDir, 'watch.txt');
fs.writeFileSync(filename, "hello");

assert.throws(
  function() {
    fs.watchFile(filename);
  },
  function(e) {
    return e.message === 'watchFile requires a listener function';
  }
);

assert.doesNotThrow(
  function() {
    fs.watchFile(filename, function(curr, prev) {
      fs.unwatchFile(filename);
      fs.unlinkSync(filename);
    });
  }
);

setTimeout(function() {
  fs.writeFileSync(filename, "world");
}, 1000);

