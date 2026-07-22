'use strict';

const common = require('../common');

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
    assert.strictEqual(event, 'change');

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
    assert.strictEqual(interval, null);
  });
})().then(common.mustCall());
