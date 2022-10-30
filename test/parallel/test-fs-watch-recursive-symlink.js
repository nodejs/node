'use strict';

const common = require('../common');
const { setTimeout } = require('timers/promises');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

// fs-watch on folders have limited capability in AIX.
// The testcase makes use of folder watching, and causes
// hang. This behavior is documented. Skip this for AIX.

if (common.isAIX)
  common.skip('folder watch capability is limited in AIX.');

const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
const testDir = tmpdir.path;
tmpdir.refresh();

(async () => {
  // Add a recursive symlink to the parent folder

  const testsubdir = fs.mkdtempSync(testDir + path.sep);
  const file = 'file-1.txt';
  const filePath = path.join(testsubdir, file);
  const watcher = fs.watch(testsubdir, { recursive: true });

  const symlinkFile = path.join(testsubdir, 'symlink-folder');
  fs.symlinkSync(testsubdir, symlinkFile);

  let watcherClosed = false;
  watcher.on('change', function(event, filename) {
    assert.ok(event === 'change' || event === 'rename');

    if (filename === file) {
      watcher.close();
      watcherClosed = true;
    }
  });

  await setTimeout(common.platformTimeout(100));
  fs.writeFileSync(filePath, 'world');

  process.once('exit', function() {
    assert(watcherClosed, 'watcher Object was not closed');
  });
})().then(common.mustCall());

(async () => {
  // Add a recursive symlink outside the parent folder

  const testsubdir = fs.mkdtempSync(testDir + path.sep);
  const file = 'file-1.txt';
  const filePath = path.join(testsubdir, file);

  const trackFolder = path.join(testsubdir, 'tracking-folder');
  const acceptableFile = 'acceptable-1.txt';
  const acceptableFilePath = path.join(trackFolder, acceptableFile);
  fs.mkdirSync(trackFolder);

  const symlinkFile = path.join(trackFolder, 'symlink-folder');
  fs.symlinkSync(testsubdir, symlinkFile);

  const watcher = fs.watch(trackFolder, { recursive: true });
  let watcherClosed = false;
  watcher.on('change', function(event, filename) {
    assert.ok(event === 'change' || event === 'rename');
    assert.ok(filename === 'symlink-folder' || filename === trackFolder || filename === acceptableFile, `Received filename ${filename}`);

    if (filename === acceptableFile) {
      watcher.close();
      watcherClosed = true;
    }
  });

  await setTimeout(common.platformTimeout(100));
  fs.writeFileSync(filePath, 'world');
  await setTimeout(common.platformTimeout(100));
  fs.writeFileSync(acceptableFilePath, 'acceptable');

  process.once('exit', function() {
    assert(watcherClosed, 'watcher Object was not closed');
  });
})().then(common.mustCall());
