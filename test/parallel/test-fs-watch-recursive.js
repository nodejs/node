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
const { pathToFileURL } = require('url');

const tmpdir = require('../common/tmpdir');
const testDir = tmpdir.path;
tmpdir.refresh();

(async () => {
  // Add a file to already watching folder

  const rootDirectory = fs.mkdtempSync(testDir + path.sep);
  const testDirectory = path.join(rootDirectory, 'test-1');
  fs.mkdirSync(testDirectory);

  const testFile = path.join(testDirectory, 'file-1.txt');

  const watcher = fs.watch(testDirectory, { recursive: true });
  let watcherClosed = false;
  watcher.on('change', function(event, filename) {
    assert.ok(event === 'rename');

    if (filename === path.basename(testFile)) {
      watcher.close();
      watcherClosed = true;
    }
  });

  await setTimeout(common.platformTimeout(100));
  fs.writeFileSync(testFile, 'world');

  process.once('exit', function() {
    assert(watcherClosed, 'watcher Object was not closed');
  });
})().then(common.mustCall());

(async () => {
  // Add a folder to already watching folder

  const rootDirectory = fs.mkdtempSync(testDir + path.sep);
  const testDirectory = path.join(rootDirectory, 'test-2');
  fs.mkdirSync(testDirectory);

  const testFile = path.join(testDirectory, 'folder-2');

  const watcher = fs.watch(testDirectory, { recursive: true });
  let watcherClosed = false;
  watcher.on('change', function(event, filename) {
    assert.ok(event === 'rename');

    if (filename === path.basename(testFile)) {
      watcher.close();
      watcherClosed = true;
    }
  });

  await setTimeout(common.platformTimeout(100));
  fs.mkdirSync(testFile);

  process.once('exit', function() {
    assert(watcherClosed, 'watcher Object was not closed');
  });
})().then(common.mustCall());

(async () => {
  // Add a file to newly created folder to already watching folder

  const rootDirectory = fs.mkdtempSync(testDir + path.sep);
  const testDirectory = path.join(rootDirectory, 'test-3');
  fs.mkdirSync(testDirectory);

  const filePath = path.join(testDirectory, 'folder-3');

  const childrenFile = 'file-4.txt';
  const childrenAbsolutePath = path.join(filePath, childrenFile);
  const childrenRelativePath = path.join(path.basename(filePath), childrenFile);

  const watcher = fs.watch(testDirectory, { recursive: true });
  let watcherClosed = false;
  watcher.on('change', function(event, filename) {
    assert.ok(event === 'rename');
    assert.ok(filename === path.basename(filePath) || filename === childrenRelativePath);

    if (filename === childrenRelativePath) {
      watcher.close();
      watcherClosed = true;
    }
  });

  await setTimeout(common.platformTimeout(100));
  fs.mkdirSync(filePath);
  await setTimeout(common.platformTimeout(100));
  fs.writeFileSync(childrenAbsolutePath, 'world');

  process.once('exit', function() {
    assert(watcherClosed, 'watcher Object was not closed');
  });
})().then(common.mustCall());

(async () => {
  // Add a file to subfolder of a watching folder

  const rootDirectory = fs.mkdtempSync(testDir + path.sep);
  const testDirectory = path.join(rootDirectory, 'test-4');
  fs.mkdirSync(testDirectory);

  const file = 'folder-5';
  const filePath = path.join(testDirectory, file);
  fs.mkdirSync(filePath);

  const subfolderPath = path.join(filePath, 'subfolder-6');
  fs.mkdirSync(subfolderPath);

  const childrenFile = 'file-7.txt';
  const childrenAbsolutePath = path.join(subfolderPath, childrenFile);
  const relativePath = path.join(file, path.basename(subfolderPath), childrenFile);

  const watcher = fs.watch(testDirectory, { recursive: true });
  let watcherClosed = false;
  watcher.on('change', function(event, filename) {
    assert.ok(event === 'rename');

    if (filename === relativePath) {
      watcher.close();
      watcherClosed = true;
    }
  });

  await setTimeout(common.platformTimeout(100));
  fs.writeFileSync(childrenAbsolutePath, 'world');

  process.once('exit', function() {
    assert(watcherClosed, 'watcher Object was not closed');
  });
})().then(common.mustCall());

(async () => {
  // Add a file to already watching folder, and use URL as the path

  const rootDirectory = fs.mkdtempSync(testDir + path.sep);
  const testDirectory = path.join(rootDirectory, 'test-5');
  fs.mkdirSync(testDirectory);

  const filePath = path.join(testDirectory, 'file-8.txt');
  const url = pathToFileURL(testDirectory);

  const watcher = fs.watch(url, { recursive: true });
  let watcherClosed = false;
  watcher.on('change', function(event, filename) {
    assert.ok(event === 'rename');

    if (filename === path.basename(filePath)) {
      watcher.close();
      watcherClosed = true;
    }
  });

  await setTimeout(common.platformTimeout(100));
  fs.writeFileSync(filePath, 'world');

  process.on('exit', function() {
    assert(watcherClosed, 'watcher Object was not closed');
  });
})().then(common.mustCall());

(async () => {
  // Watch a file (not a folder) using fs.watch

  const rootDirectory = fs.mkdtempSync(testDir + path.sep);
  const testDirectory = path.join(rootDirectory, 'test-6');
  fs.mkdirSync(testDirectory);

  const filePath = path.join(testDirectory, 'only-file.txt');
  fs.writeFileSync(filePath, 'hello');

  const watcher = fs.watch(filePath, { recursive: true });
  let watcherClosed = false;
  let interval;
  watcher.on('change', function(event, filename) {
    assert.ok(event === 'change');

    if (filename === path.basename(filePath)) {
      clearInterval(interval);
      interval = null;
      watcher.close();
      watcherClosed = true;
    }
  });

  interval = setInterval(() => {
    fs.writeFileSync(filePath, 'world');
  }, common.platformTimeout(10));

  process.on('exit', function() {
    assert(watcherClosed, 'watcher Object was not closed');
    assert.ok(interval === null, 'interval should have been null');
  });
})().then(common.mustCall());


(async () => {
  // Assert recursive watch does not leak handles
  const rootDirectory = fs.mkdtempSync(testDir + path.sep);
  const testDirectory = path.join(rootDirectory, 'test-7');
  const filePath = path.join(testDirectory, 'only-file.txt');
  fs.mkdirSync(testDirectory);

  let watcherClosed = false;
  const watcher = fs.watch(testDirectory, { recursive: true });
  watcher.on('change', common.mustCallAtLeast(async (event, filename) => {
    await setTimeout(common.platformTimeout(100));
    if (filename === path.basename(filePath)) {
      watcher.close();
      watcherClosed = true;
    }
    await setTimeout(common.platformTimeout(100));
    assert(!process._getActiveHandles().some((handle) => handle.constructor.name === 'StatWatcher'));
  }));

  process.on('exit', function() {
    assert(watcherClosed, 'watcher Object was not closed');
  });
  await setTimeout(common.platformTimeout(100));
  fs.writeFileSync(filePath, 'content');
})().then(common.mustCall());

(async () => {
  // Handle non-boolean values for options.recursive

  if (!common.isWindows && !common.isOSX) {
    assert.throws(() => {
      const testsubdir = fs.mkdtempSync(testDir + path.sep);
      fs.watch(testsubdir, { recursive: '1' });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  }
})().then(common.mustCall());
