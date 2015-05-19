'use strict';
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

var openCount = 0;
var _fsopen = fs.open;
var _fsclose = fs.close;

var loopCount = 50;
var emptyTxt = path.join(common.fixturesDir, 'empty.txt');

fs.open = function() {
  openCount++;
  return _fsopen.apply(null, arguments);
};

fs.close = function() {
  openCount--;
  return _fsclose.apply(null, arguments);
};

function testLeak(endFn, callback) {
  console.log('testing for leaks from fs.createReadStream().%s()...', endFn);

  var i = 0;

  setInterval(function() {
    var s = fs.createReadStream(emptyTxt);
    s[endFn]();

    if (++i === loopCount) {
      clearTimeout(this);
      setTimeout(function() {
        assert.equal(0, openCount, 'no leaked file descriptors using ' +
                     endFn + '() (got ' + openCount + ')');
        openCount = 0;
        callback && setTimeout(callback, 100);
      }, 100);
    }
  }, 2);
}

testLeak('close', function() {
  testLeak('destroy');
});
