'use strict';

const common = require('../common');
const { setTimeout } = require('timers/promises');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

const { randomUUID } = require('crypto');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
const testDir = tmpdir.path;
tmpdir.refresh();

(async () => {
  {
    // Add a file to already watching folder

    const testsubdir = fs.mkdtempSync(testDir + path.sep);
    const file = `${randomUUID()}.txt`;
    const filePath = path.join(testsubdir, file);
    const watcher = fs.watch(testsubdir, { recursive: true });

    let watcherClosed = false;
    watcher.on('change', function(event, filename) {
      assert.ok(event === 'change' || event === 'rename');

      if (filename === file) {
        watcher.close();
        watcherClosed = true;
      }
    });

    await setTimeout(100);
    fs.writeFileSync(filePath, 'world');

    process.on('exit', function() {
      assert(watcherClosed, 'watcher Object was not closed');
    });
  }

  {
    // Add a folder to already watching folder

    const testsubdir = fs.mkdtempSync(testDir + path.sep);
    const file = `folder-${randomUUID()}`;
    const filePath = path.join(testsubdir, file);
    const watcher = fs.watch(testsubdir, { recursive: true });

    let watcherClosed = false;
    watcher.on('change', function(event, filename) {
      assert.ok(event === 'change' || event === 'rename');

      if (filename === file) {
        watcher.close();
        watcherClosed = true;
      }
    });

    await setTimeout(100);
    fs.mkdirSync(filePath);

    process.on('exit', function() {
      assert(watcherClosed, 'watcher Object was not closed');
    });
  }

  {
    // Add a file to newly created folder to already watching folder

    const testsubdir = fs.mkdtempSync(testDir + path.sep);
    const file = `folder-${randomUUID()}`;
    const filePath = path.join(testsubdir, file);
    const watcher = fs.watch(testsubdir, { recursive: true });
    const childrenFile = `file-${randomUUID()}.txt`;
    const childrenAbsolutePath = path.join(filePath, childrenFile);

    let watcherClosed = false;

    watcher.on('change', function(event, filename) {
      assert.ok(event === 'change' || event === 'rename');

      if (filename === path.join(file, childrenFile)) {
        watcher.close();
        watcherClosed = true;
      }
    });

    await setTimeout(100);
    fs.mkdirSync(filePath);
    await setTimeout(100);
    fs.writeFileSync(childrenAbsolutePath, 'world');

    process.on('exit', function() {
      assert(watcherClosed, 'watcher Object was not closed');
    });
  }

  {
    // Add a file to subfolder of a watching folder

    const testsubdir = fs.mkdtempSync(testDir + path.sep);
    const file = `folder-${randomUUID()}`;
    const filePath = path.join(testsubdir, file);
    fs.mkdirSync(filePath);

    const subFolder = `subfolder-${randomUUID()}`;
    const subfolderPath = path.join(filePath, subFolder);

    fs.mkdirSync(subfolderPath);

    const watcher = fs.watch(testsubdir, { recursive: true });
    const childrenFile = `file-${randomUUID()}.txt`;
    const childrenAbsolutePath = path.join(subfolderPath, childrenFile);
    const relativePath = path.join(file, subFolder, childrenFile);

    let watcherClosed = false;

    watcher.on('change', function(event, filename) {
      assert.ok(event === 'change' || event === 'rename');

      if (filename === relativePath) {
        watcher.close();
        watcherClosed = true;
      }
    });

    await setTimeout(100);
    fs.writeFileSync(childrenAbsolutePath, 'world');

    process.on('exit', function() {
      assert(watcherClosed, 'watcher Object was not closed');
    });
  }
})().then(common.mustCall());
