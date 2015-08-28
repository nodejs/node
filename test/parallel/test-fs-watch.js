'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

const expectFilePath = common.isWindows ||
                     process.platform === 'linux' ||
                     process.platform === 'darwin';
const testDir = common.tmpDir;
const subdir = path.join(testDir, 'testsubdir');
const fileOne = path.join(testDir, 'watch.txt');
const fileTwo = path.join(testDir, 'hasOwnProperty');
const fileThree = path.join(subdir, 'newfile.txt');

common.refreshTmpDir();

// Test the change event handler
fs.writeFileSync(fileOne, 'hello');
const watcher1 = fs.watch(fileOne);
watcher1.on('change', common.mustCall(function(event, filename) {
  watcher1.close();
  assert.equal('change', event);
  assert.equal(expectFilePath ? 'watch.txt' : null, filename);
}));

/*
 * This timeouts after the watchers are necessary as FreeBSD and OSX expect some
 * time delay before starting the actual watching. They are there, as the builds
 * were failing. Do not remove them.
 */
setTimeout(() => fs.writeFileSync(fileOne, 'world'), 20);

// Test the change event listener
fs.writeFileSync(fileTwo, 'hello');
const watcher2 = fs.watch(fileTwo, common.mustCall(listener1));
function listener1(evt, filename) {
  watcher2.close();
  assert.equal('change', evt);
  assert.equal(expectFilePath ? 'hasOwnProperty' : null, filename);
}
setTimeout(() => fs.writeFileSync(fileTwo, 'world'), 20);

// Test the change event listener on a directory
fs.mkdirSync(subdir, 0o700);
const watcher3 = fs.watch(subdir, common.mustCall(listener2));
function listener2(evt, filename) {
  watcher3.close();
  const renameEv = process.platform === 'sunos' ? 'change' : 'rename';
  assert.equal(renameEv, evt);
  assert.equal(expectFilePath ? 'newfile.txt' : null, filename);
}
setTimeout(() => fs.closeSync(fs.openSync(fileThree, 'w')), 20);

// fs.watch throws if the file does not exist
assert.throws(function() {
  fs.watch('non-existent-file', assert.fail);
}, /watch ENOENT/);

// https://github.com/joyent/node/issues/2293 - non-persistent watcher should
// not block the event loop
fs.watch(__filename, {persistent: false}, assert.fail);

// whitebox test to ensure that wrapped FSEvent is safe
// https://github.com/joyent/node/issues/6690
var oldhandle;
assert.throws(function() {
  var w = fs.watch(__filename, function(event, filename) { });
  oldhandle = w._handle;
  w._handle = { close: w._handle.close };
  w.close();
}, TypeError);
oldhandle.close(); // clean up
