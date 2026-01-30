'use strict';

const common = require('../common');
const { skipIfNoWatch } = require('../common/watch.js');

skipIfNoWatch();

const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const testFileName = 'keep.txt';
const testFilePath = path.join(tmpdir.path, testFileName);
const ignoredLogName = 'debug.log';
const ignoredLogPath = path.join(tmpdir.path, ignoredLogName);
const ignoredTmpName = 'temp.tmp';
const ignoredTmpPath = path.join(tmpdir.path, ignoredTmpName);
const ignoredHiddenName = '.secret';
const ignoredHiddenPath = path.join(tmpdir.path, ignoredHiddenName);

const watcher = fs.watch(tmpdir.path, {
  ignore: [
    '*.log',
    /\.tmp$/,
    (filename) => filename.startsWith('.'),
  ],
});

watcher.on('change', common.mustCallAtLeast((event, filename) => {
  assert.notStrictEqual(filename, ignoredLogName);
  assert.notStrictEqual(filename, ignoredTmpName);
  assert.notStrictEqual(filename, ignoredHiddenName);

  if (filename === testFileName) {
    watcher.close();
  }
}, 1));

function writeFiles() {
  fs.writeFileSync(ignoredLogPath, 'ignored');
  fs.writeFileSync(ignoredTmpPath, 'ignored');
  fs.writeFileSync(ignoredHiddenPath, 'ignored');
  fs.writeFileSync(testFilePath, 'content');
}

if (common.isMacOS) {
  // Do the write with a delay to ensure that the OS is ready to notify us. See
  // https://github.com/nodejs/node/issues/52601.
  setTimeout(writeFiles, common.platformTimeout(100));
} else {
  writeFiles();
}
