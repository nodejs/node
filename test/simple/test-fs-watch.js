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

var expectFilePath = process.platform == 'win32' || process.platform == 'linux';

var watchSeenOne = 0;
var watchSeenTwo = 0;
var watchSeenThree = 0;

var testDir = common.tmpDir;

var filenameOne = 'watch.txt';
var filepathOne = path.join(testDir, filenameOne);

var filenameTwo = 'hasOwnProperty';
var filepathTwo = filenameTwo;
var filepathTwoAbs = path.join(testDir, filenameTwo);

var filenameThree = 'newfile.txt';
var testsubdir = path.join(testDir, 'testsubdir');
var filepathThree = path.join(testsubdir, filenameThree);


process.on('exit', function() {
  assert.ok(watchSeenOne > 0);
  assert.ok(watchSeenTwo > 0);
  assert.ok(watchSeenThree > 0);
});

// Clean up stale files (if any) from previous run.
try { fs.unlinkSync(filepathOne);    } catch (e) { }
try { fs.unlinkSync(filepathTwoAbs); } catch (e) { }
try { fs.unlinkSync(filepathThree);  } catch (e) { }
try { fs.rmdirSync(testsubdir);      } catch (e) { }

fs.writeFileSync(filepathOne, 'hello');

assert.throws(
    function() {
      fs.watch(filepathOne);
    },
    function(e) {
      return e.message === 'watch requires a listener function';
    }
);

assert.doesNotThrow(
    function() {
      var watcher = fs.watch(filepathOne, function(event, filename) {
        assert.equal('change', event);
        if (expectFilePath) {
          assert.equal('watch.txt', filename);
        } else {
          assert.equal(null, filename);
        }
        watcher.close();
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
      fs.watch(filepathTwo);
    },
    function(e) {
      return e.message === 'watch requires a listener function';
    }
);

assert.doesNotThrow(
    function() {
      var watcher = fs.watch(filepathTwo, function(event, filename) {
        assert.equal('change', event);
        if (expectFilePath) {
          assert.equal('hasOwnProperty', filename);
        } else {
          assert.equal(null, filename);
        }
        watcher.close();
        ++watchSeenTwo;
      });
    }
);

setTimeout(function() {
  fs.writeFileSync(filepathTwoAbs, 'pardner');
}, 1000);

try { fs.unlinkSync(filepathThree); } catch (e) {}
try { fs.mkdirSync(testsubdir, 0700); } catch (e) {}

assert.doesNotThrow(
    function() {
      var watcher = fs.watch(testsubdir, function(event, filename) {
        assert.equal('rename', event);
        if (expectFilePath) {
          assert.equal('newfile.txt', filename);
        } else {
          assert.equal(null, filename);
        }
        watcher.close();
        ++watchSeenThree;
      });
    }
);

setTimeout(function() {
  var fd = fs.openSync(filepathThree, 'w');
  fs.closeSync(fd);
}, 1000);

// https://github.com/joyent/node/issues/2293 - non-persistent watcher should
// not block the event loop
fs.watch(__filename, {persistent: false}, function() {
  assert(0);
});
