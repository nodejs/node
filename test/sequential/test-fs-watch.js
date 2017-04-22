'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

const expectFilePath = common.isWindows ||
                       common.isLinux ||
                       common.isOSX ||
                       common.isAix;

let watchSeenOne = 0;
let watchSeenTwo = 0;
let watchSeenThree = 0;

const testDir = common.tmpDir;

const filenameOne = 'watch.txt';
const filepathOne = path.join(testDir, filenameOne);

const filenameTwo = 'hasOwnProperty';
const filepathTwo = filenameTwo;
const filepathTwoAbs = path.join(testDir, filenameTwo);

process.on('exit', function() {
  assert.ok(watchSeenOne > 0);
  assert.ok(watchSeenTwo > 0);
  assert.ok(watchSeenThree > 0);
});

common.refreshTmpDir();

fs.writeFileSync(filepathOne, 'hello');

assert.doesNotThrow(
    function() {
      const watcher = fs.watch(filepathOne);
      watcher.on('change', function(event, filename) {
        assert.strictEqual(event, 'change');

        if (expectFilePath) {
          assert.strictEqual(filename, 'watch.txt');
        }
        watcher.close();
        ++watchSeenOne;
      });
    }
);

setImmediate(function() {
  fs.writeFileSync(filepathOne, 'world');
});


process.chdir(testDir);

fs.writeFileSync(filepathTwoAbs, 'howdy');

assert.doesNotThrow(
    function() {
      const watcher = fs.watch(filepathTwo, function(event, filename) {
        assert.strictEqual(event, 'change');

        if (expectFilePath) {
          assert.strictEqual(filename, 'hasOwnProperty');
        }
        watcher.close();
        ++watchSeenTwo;
      });
    }
);

setImmediate(function() {
  fs.writeFileSync(filepathTwoAbs, 'pardner');
});

const filenameThree = 'newfile.txt';
const testsubdir = fs.mkdtempSync(testDir + path.sep);
const filepathThree = path.join(testsubdir, filenameThree);

assert.doesNotThrow(
    function() {
      const watcher = fs.watch(testsubdir, function(event, filename) {
        const renameEv = common.isSunOS || common.isAix ? 'change' : 'rename';
        assert.strictEqual(event, renameEv);
        if (expectFilePath) {
          assert.strictEqual(filename, 'newfile.txt');
        } else {
          assert.strictEqual(filename, null);
        }
        watcher.close();
        ++watchSeenThree;
      });
    }
);

setImmediate(function() {
  const fd = fs.openSync(filepathThree, 'w');
  fs.closeSync(fd);
});

// https://github.com/joyent/node/issues/2293 - non-persistent watcher should
// not block the event loop
fs.watch(__filename, {persistent: false}, function() {
  assert(0);
});

// whitebox test to ensure that wrapped FSEvent is safe
// https://github.com/joyent/node/issues/6690
let oldhandle;
assert.throws(function() {
  const w = fs.watch(__filename, function(event, filename) { });
  oldhandle = w._handle;
  w._handle = { close: w._handle.close };
  w.close();
}, /^TypeError: Illegal invocation$/);
oldhandle.close(); // clean up

assert.throws(function() {
  const w = fs.watchFile(__filename, {persistent: false}, function() {});
  oldhandle = w._handle;
  w._handle = { stop: w._handle.stop };
  w.stop();
}, /^TypeError: Illegal invocation$/);
oldhandle.stop(); // clean up
