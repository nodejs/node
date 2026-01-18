'use strict';

const common = require('../common');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

if (common.isAIX)
  common.skip('folder watch capability is limited in AIX.');

const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
const testDir = tmpdir.path;
tmpdir.refresh();

// Test 1: String glob pattern ignore
{
  const rootDirectory = fs.mkdtempSync(testDir + path.sep);
  const testDirectory = path.join(rootDirectory, 'test-glob');
  fs.mkdirSync(testDirectory);

  const testFile = path.join(testDirectory, 'file.txt');
  const ignoredFile = path.join(testDirectory, 'file.log');

  const watcher = fs.watch(testDirectory, {
    ignore: '*.log',
  });

  let seenFile = false;
  let seenIgnored = false;
  watcher.on('change', common.mustCallAtLeast((event, filename) => {
    if (filename === 'file.txt') {
      seenFile = true;
    }
    if (filename === 'file.log') {
      seenIgnored = true;
    }
    // Close watcher after we've seen the non-ignored file
    if (seenFile) {
      watcher.close();
    }
  }, 1));

  setTimeout(() => {
    fs.writeFileSync(ignoredFile, 'ignored');
    fs.writeFileSync(testFile, 'content');
  }, common.platformTimeout(200));

  process.on('exit', () => {
    assert.strictEqual(seenFile, true);
    assert.strictEqual(seenIgnored, false);
  });
}

// Test 2: RegExp ignore pattern
{
  const rootDirectory = fs.mkdtempSync(testDir + path.sep);
  const testDirectory = path.join(rootDirectory, 'test-regexp');
  fs.mkdirSync(testDirectory);

  const testFile = path.join(testDirectory, 'keep.txt');
  const ignoredFile = path.join(testDirectory, 'ignore.tmp');

  const watcher = fs.watch(testDirectory, {
    ignore: /\.tmp$/,
  });

  let seenFile = false;
  let seenIgnored = false;
  watcher.on('change', common.mustCallAtLeast((event, filename) => {
    if (filename === 'keep.txt') {
      seenFile = true;
    }
    if (filename === 'ignore.tmp') {
      seenIgnored = true;
    }
    if (seenFile) {
      watcher.close();
    }
  }, 1));

  setTimeout(() => {
    fs.writeFileSync(ignoredFile, 'ignored');
    fs.writeFileSync(testFile, 'content');
  }, common.platformTimeout(200));

  process.on('exit', () => {
    assert.strictEqual(seenFile, true);
    assert.strictEqual(seenIgnored, false);
  });
}

// Test 3: Function ignore pattern
{
  const rootDirectory = fs.mkdtempSync(testDir + path.sep);
  const testDirectory = path.join(rootDirectory, 'test-function');
  fs.mkdirSync(testDirectory);

  const testFile = path.join(testDirectory, 'visible.txt');
  const ignoredFile = path.join(testDirectory, '.hidden');

  const watcher = fs.watch(testDirectory, {
    ignore: (filename) => filename.startsWith('.'),
  });

  let seenFile = false;
  let seenIgnored = false;
  watcher.on('change', common.mustCallAtLeast((event, filename) => {
    if (filename === 'visible.txt') {
      seenFile = true;
    }
    if (filename === '.hidden') {
      seenIgnored = true;
    }
    if (seenFile) {
      watcher.close();
    }
  }, 1));

  setTimeout(() => {
    fs.writeFileSync(ignoredFile, 'ignored');
    fs.writeFileSync(testFile, 'content');
  }, common.platformTimeout(200));

  process.on('exit', () => {
    assert.strictEqual(seenFile, true);
    assert.strictEqual(seenIgnored, false);
  });
}

// Test 4: Array of mixed patterns
{
  const rootDirectory = fs.mkdtempSync(testDir + path.sep);
  const testDirectory = path.join(rootDirectory, 'test-array');
  fs.mkdirSync(testDirectory);

  const testFile = path.join(testDirectory, 'keep.txt');
  const ignoredLog = path.join(testDirectory, 'debug.log');
  const ignoredTmp = path.join(testDirectory, 'temp.tmp');
  const ignoredHidden = path.join(testDirectory, '.secret');

  const watcher = fs.watch(testDirectory, {
    ignore: [
      '*.log',
      /\.tmp$/,
      (filename) => filename.startsWith('.'),
    ],
  });

  let seenFile = false;
  let seenLog = false;
  let seenTmp = false;
  let seenHidden = false;
  watcher.on('change', common.mustCallAtLeast((event, filename) => {
    if (filename === 'keep.txt') {
      seenFile = true;
    }
    if (filename === 'debug.log') seenLog = true;
    if (filename === 'temp.tmp') seenTmp = true;
    if (filename === '.secret') seenHidden = true;

    if (seenFile) {
      watcher.close();
    }
  }, 1));

  setTimeout(() => {
    fs.writeFileSync(ignoredLog, 'ignored');
    fs.writeFileSync(ignoredTmp, 'ignored');
    fs.writeFileSync(ignoredHidden, 'ignored');
    fs.writeFileSync(testFile, 'content');
  }, common.platformTimeout(200));

  process.on('exit', () => {
    assert.strictEqual(seenFile, true);
    assert.strictEqual(seenLog, false);
    assert.strictEqual(seenTmp, false);
    assert.strictEqual(seenHidden, false);
  });
}

// Test 5: Invalid option validation
{
  assert.throws(
    () => fs.watch('.', { ignore: 123 }),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );

  assert.throws(
    () => fs.watch('.', { ignore: '' }),
    {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
    }
  );

  assert.throws(
    () => fs.watch('.', { ignore: [123] }),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );

  assert.throws(
    () => fs.watch('.', { ignore: [''] }),
    {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
    }
  );
}
