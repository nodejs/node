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

var watchSeenOne = 0;
var watchSeenTwo = 0;

var startDir = process.cwd();
var testDir = common.fixturesDir;

var filenameOne = 'watch.txt';
var filepathOne = path.join(testDir, filenameOne);

var filenameTwo = 'hasOwnProperty';
var filepathTwo = filenameTwo;
var filepathTwoAbs = path.join(testDir, filenameTwo);


process.on('exit', function() {
  fs.unlinkSync(filepathOne);
  fs.unlinkSync(filepathTwoAbs);
  assert.equal(1, watchSeenOne);
  assert.equal(1, watchSeenTwo);
});


fs.writeFileSync(filepathOne, 'hello');

assert.throws(
    function() {
      fs.watchFile(filepathOne);
    },
    function(e) {
      return e.message === 'watchFile requires a listener function';
    }
);

assert.doesNotThrow(
    function() {
      fs.watchFile(filepathOne, function(curr, prev) {
        fs.unwatchFile(filepathOne);
        ++watchSeenOne;
      });
    }
);

setTimeout(function() {
  fs.writeFileSync(filepathOne, 'world');
}, 1000);


process.chdir(testDir);

fs.writeFileSync(filepathTwoAbs, 'howdy');

assert.throws(
    function() {
      fs.watchFile(filepathTwo);
    },
    function(e) {
      return e.message === 'watchFile requires a listener function';
    }
);

assert.doesNotThrow(
    function() {
      fs.watchFile(filepathTwo, function(curr, prev) {
        fs.unwatchFile(filepathTwo);
        ++watchSeenTwo;
      });
    }
);

setTimeout(function() {
  fs.writeFileSync(filepathTwoAbs, 'pardner');
}, 1000);
