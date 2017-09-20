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

'use strict';
const common = require('../common');

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const expectFilePath = common.isWindows ||
                       common.isLinux ||
                       common.isOSX ||
                       common.isAIX;

const testDir = common.tmpDir;

common.refreshTmpDir();

{
  const filepath = path.join(testDir, 'watch.txt');

  fs.writeFileSync(filepath, 'hello');

  assert.doesNotThrow(
    function() {
      const watcher = fs.watch(filepath);
      watcher.on('change', common.mustCall(function(event, filename) {
        assert.strictEqual(event, 'change');

        if (expectFilePath) {
          assert.strictEqual(filename, 'watch.txt');
        }
        watcher.close();
      }));
    }
  );

  setImmediate(function() {
    fs.writeFileSync(filepath, 'world');
  });
}

{
  const filepathAbs = path.join(testDir, 'hasOwnProperty');

  process.chdir(testDir);

  fs.writeFileSync(filepathAbs, 'howdy');

  assert.doesNotThrow(
    function() {
      const watcher =
        fs.watch('hasOwnProperty', common.mustCall(function(event, filename) {
          assert.strictEqual(event, 'change');

          if (expectFilePath) {
            assert.strictEqual(filename, 'hasOwnProperty');
          }
          watcher.close();
        }));
    }
  );

  setImmediate(function() {
    fs.writeFileSync(filepathAbs, 'pardner');
  });
}

{
  const testsubdir = fs.mkdtempSync(testDir + path.sep);
  const filepath = path.join(testsubdir, 'newfile.txt');

  assert.doesNotThrow(
    function() {
      const watcher =
        fs.watch(testsubdir, common.mustCall(function(event, filename) {
          const renameEv = common.isSunOS || common.isAIX ? 'change' : 'rename';
          assert.strictEqual(event, renameEv);
          if (expectFilePath) {
            assert.strictEqual(filename, 'newfile.txt');
          } else {
            assert.strictEqual(filename, null);
          }
          watcher.close();
        }));
    }
  );

  setImmediate(function() {
    const fd = fs.openSync(filepath, 'w');
    fs.closeSync(fd);
  });

}

// https://github.com/joyent/node/issues/2293 - non-persistent watcher should
// not block the event loop
{
  fs.watch(__filename, { persistent: false }, common.mustNotCall());
}

// whitebox test to ensure that wrapped FSEvent is safe
// https://github.com/joyent/node/issues/6690
{
  let oldhandle;
  assert.throws(function() {
    const w = fs.watch(__filename, common.mustNotCall());
    oldhandle = w._handle;
    w._handle = { close: w._handle.close };
    w.close();
  }, /^TypeError: Illegal invocation$/);
  oldhandle.close(); // clean up

  assert.throws(function() {
    const w = fs.watchFile(__filename, { persistent: false },
                           common.mustNotCall());
    oldhandle = w._handle;
    w._handle = { stop: w._handle.stop };
    w.stop();
  }, /^TypeError: Illegal invocation$/);
  oldhandle.stop(); // clean up
}
