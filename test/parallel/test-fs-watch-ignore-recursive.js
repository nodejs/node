'use strict';

const common = require('../common');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

if (common.isAIX)
  common.skip('folder watch capability is limited in AIX.');

if (common.isSunOS)
  common.skip('fs.watch is not reliable on SunOS.');

const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
const testDir = tmpdir.path;
tmpdir.refresh();

// Test 1: String glob pattern ignore with recursive watch
{
  const rootDirectory = fs.mkdtempSync(testDir + path.sep);
  const testDirectory = path.join(rootDirectory, 'test-recursive-glob');
  const subDirectory = path.join(testDirectory, 'subdir');
  fs.mkdirSync(testDirectory);
  fs.mkdirSync(subDirectory);

  const testFile = path.join(subDirectory, 'file.txt');
  const ignoredFile = path.join(subDirectory, 'file.log');

  const watcher = fs.watch(testDirectory, {
    recursive: true,
    ignore: '*.log',
  });

  let seenFile = false;
  let seenIgnored = false;
  let interval;

  watcher.on('change', common.mustCallAtLeast((event, filename) => {
    // On recursive watch, filename includes relative path from watched dir
    if (filename && filename.endsWith('file.txt')) {
      seenFile = true;
    }
    if (filename && filename.endsWith('file.log')) {
      seenIgnored = true;
    }
    if (seenFile) {
      clearInterval(interval);
      interval = null;
      watcher.close();
    }
  }, 1));

  // Use setInterval to handle potential event delays on macOS FSEvents
  process.nextTick(common.mustCall(() => {
    interval = setInterval(() => {
      fs.writeFileSync(ignoredFile, 'ignored');
      fs.writeFileSync(testFile, 'content-' + Date.now());
    }, 100);
  }));

  process.on('exit', () => {
    assert.strictEqual(interval, null);
    assert.strictEqual(seenFile, true);
    assert.strictEqual(seenIgnored, false);
  });
}

// Test 2: RegExp ignore pattern with recursive watch
{
  const rootDirectory = fs.mkdtempSync(testDir + path.sep);
  const testDirectory = path.join(rootDirectory, 'test-recursive-regexp');
  const subDirectory = path.join(testDirectory, 'nested');
  fs.mkdirSync(testDirectory);
  fs.mkdirSync(subDirectory);

  const testFile = path.join(subDirectory, 'keep.txt');
  const ignoredFile = path.join(subDirectory, 'temp.tmp');

  const watcher = fs.watch(testDirectory, {
    recursive: true,
    ignore: /\.tmp$/,
  });

  let seenFile = false;
  let seenIgnored = false;
  let interval;

  watcher.on('change', common.mustCallAtLeast((event, filename) => {
    if (filename && filename.endsWith('keep.txt')) {
      seenFile = true;
    }
    if (filename && filename.endsWith('temp.tmp')) {
      seenIgnored = true;
    }
    if (seenFile) {
      clearInterval(interval);
      interval = null;
      watcher.close();
    }
  }, 1));

  // Use setInterval to handle potential event delays on macOS FSEvents
  process.nextTick(common.mustCall(() => {
    interval = setInterval(() => {
      fs.writeFileSync(ignoredFile, 'ignored');
      fs.writeFileSync(testFile, 'content-' + Date.now());
    }, 100);
  }));

  process.on('exit', () => {
    assert.strictEqual(interval, null);
    assert.strictEqual(seenFile, true);
    assert.strictEqual(seenIgnored, false);
  });
}

// Test 3: Glob pattern with ** to match paths in subdirectories
{
  const rootDirectory = fs.mkdtempSync(testDir + path.sep);
  const testDirectory = path.join(rootDirectory, 'test-recursive-globstar');
  const nodeModules = path.join(testDirectory, 'node_modules');
  const srcDir = path.join(testDirectory, 'src');
  fs.mkdirSync(testDirectory);
  fs.mkdirSync(nodeModules);
  fs.mkdirSync(srcDir);

  const testFile = path.join(srcDir, 'app.js');
  const ignoredFile = path.join(nodeModules, 'package.json');

  const watcher = fs.watch(testDirectory, {
    recursive: true,
    // On Linux, matching the directory skips watching it entirely.
    // On macOS, the native watcher still needs to filter file events inside.
    ignore: ['**/node_modules/**', '**/node_modules'],
  });

  let seenFile = false;
  let seenIgnored = false;
  let interval;

  watcher.on('change', common.mustCallAtLeast((event, filename) => {
    if (filename && filename.endsWith('app.js')) {
      seenFile = true;
    }
    if (filename && filename.includes('node_modules')) {
      seenIgnored = true;
    }
    if (seenFile) {
      clearInterval(interval);
      interval = null;
      watcher.close();
    }
  }, 1));

  // Use setInterval to handle potential event delays on macOS FSEvents
  process.nextTick(common.mustCall(() => {
    interval = setInterval(() => {
      fs.writeFileSync(ignoredFile, '{}');
      fs.writeFileSync(testFile, 'console.log("hello-' + Date.now() + '")');
    }, 100);
  }));

  process.on('exit', () => {
    assert.strictEqual(interval, null);
    assert.strictEqual(seenFile, true);
    assert.strictEqual(seenIgnored, false);
  });
}

// Test 4: Array of mixed patterns with recursive watch
{
  const rootDirectory = fs.mkdtempSync(testDir + path.sep);
  const testDirectory = path.join(rootDirectory, 'test-recursive-array');
  const subDirectory = path.join(testDirectory, 'deep');
  fs.mkdirSync(testDirectory);
  fs.mkdirSync(subDirectory);

  const testFile = path.join(subDirectory, 'visible.txt');
  const ignoredLog = path.join(subDirectory, 'debug.log');
  const ignoredTmp = path.join(subDirectory, 'temp.tmp');
  const ignoredHidden = path.join(subDirectory, '.gitignore');

  const watcher = fs.watch(testDirectory, {
    recursive: true,
    ignore: [
      '*.log',
      /\.tmp$/,
      (filename) => path.basename(filename).startsWith('.'),
    ],
  });

  let seenFile = false;
  let seenLog = false;
  let seenTmp = false;
  let seenHidden = false;
  let interval;

  watcher.on('change', common.mustCallAtLeast((event, filename) => {
    if (filename && filename.endsWith('visible.txt')) {
      seenFile = true;
    }
    if (filename && filename.endsWith('debug.log')) seenLog = true;
    if (filename && filename.endsWith('temp.tmp')) seenTmp = true;
    if (filename && filename.endsWith('.gitignore')) seenHidden = true;

    if (seenFile) {
      clearInterval(interval);
      interval = null;
      watcher.close();
    }
  }, 1));

  // Use setInterval to handle potential event delays on macOS FSEvents
  process.nextTick(common.mustCall(() => {
    interval = setInterval(() => {
      fs.writeFileSync(ignoredLog, 'ignored');
      fs.writeFileSync(ignoredTmp, 'ignored');
      fs.writeFileSync(ignoredHidden, 'ignored');
      fs.writeFileSync(testFile, 'content-' + Date.now());
    }, 100);
  }));

  process.on('exit', () => {
    assert.strictEqual(interval, null);
    assert.strictEqual(seenFile, true);
    assert.strictEqual(seenLog, false);
    assert.strictEqual(seenTmp, false);
    assert.strictEqual(seenHidden, false);
  });
}
