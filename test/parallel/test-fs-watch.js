'use strict';
const common = require('../common');

// tests if `filename` is provided to watcher on supported platforms

const fs = require('fs');
const assert = require('assert');
const { join } = require('path');

class WatchTestCase {
  constructor(shouldInclude, dirName, fileName, field) {
    this.dirName = dirName;
    this.fileName = fileName;
    this.field = field;
    this.shouldSkip = !shouldInclude;
  }
  get dirPath() { return join(common.tmpDir, this.dirName); }
  get filePath() { return join(this.dirPath, this.fileName); }
}

const cases = [
  // Watch on a directory should callback with a filename on supported systems
  new WatchTestCase(
    common.isLinux || common.isOSX || common.isWindows || common.isAIX,
    'watch1',
    'foo',
    'filePath'
  ),
  // Watch on a file should callback with a filename on supported systems
  new WatchTestCase(
    common.isLinux || common.isOSX || common.isWindows,
    'watch2',
    'bar',
    'dirPath'
  )
];

common.refreshTmpDir();

for (const testCase of cases) {
  if (testCase.shouldSkip) continue;
  fs.mkdirSync(testCase.dirPath);
  // long content so it's actually flushed.
  const content1 = Date.now() + testCase.fileName.toLowerCase().repeat(1e4);
  fs.writeFileSync(testCase.filePath, content1);

  let interval;
  const watcher = fs.watch(testCase[testCase.field]);
  watcher.on('error', (err) => {
    if (interval) {
      clearInterval(interval);
      interval = null;
    }
    assert.fail(err);
  });
  watcher.on('change', common.mustCall(function(eventType, argFilename) {
    if (interval) {
      clearInterval(interval);
      interval = null;
    }
    if (common.isOSX)
      assert.strictEqual(['rename', 'change'].includes(eventType), true);
    else
      assert.strictEqual(eventType, 'change');
    assert.strictEqual(argFilename, testCase.fileName);

    watcher.start();  // should not crash

    // end of test case
    watcher.close();
  }));

  // long content so it's actually flushed. toUpperCase so there's real change.
  const content2 = Date.now() + testCase.fileName.toUpperCase().repeat(1e4);
  interval = setInterval(() => {
    fs.writeFileSync(testCase.filePath, '');
    fs.writeFileSync(testCase.filePath, content2);
  }, 100);
}
