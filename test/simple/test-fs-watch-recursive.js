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

if (process.platform === 'darwin') {
  var watchSeenOne = 0;

  var testDir = common.tmpDir;

  var filenameOne = 'watch.txt';
  var testsubdir = path.join(testDir, 'testsubdir');
  var relativePathOne = path.join('testsubdir', filenameOne);
  var filepathOne = path.join(testsubdir, filenameOne);

  process.on('exit', function() {
    assert.ok(watchSeenOne > 0);
  });

  function cleanup() {
    try { fs.unlinkSync(filepathOne); } catch (e) { }
    try { fs.rmdirSync(testsubdir); } catch (e) { }
  };

  try { fs.mkdirSync(testsubdir, 0700); } catch (e) {}
  fs.writeFileSync(filepathOne, 'hello');

  assert.doesNotThrow(function() {
    var watcher = fs.watch(testDir, {recursive: true});
    watcher.on('change', function(event, filename) {
      assert.ok('change' === event || 'rename' === event);
      assert.equal(relativePathOne, filename);

      watcher.close();
      cleanup();
      ++watchSeenOne;
    });
  });

  setTimeout(function() {
    fs.writeFileSync(filepathOne, 'world');
  }, 10);
}
