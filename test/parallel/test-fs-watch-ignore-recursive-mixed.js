'use strict';

const common = require('../common');
const { skipIfNoWatch } = require('../common/watch.js');

skipIfNoWatch();

const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const subDirectory = path.join(tmpdir.path, 'deep');
fs.mkdirSync(subDirectory);

const testFileName = 'visible.txt';
const testFilePath = path.join(subDirectory, testFileName);
const ignoredLogName = 'debug.log';
const ignoredLogPath = path.join(subDirectory, ignoredLogName);
const ignoredTmpName = 'temp.tmp';
const ignoredTmpPath = path.join(subDirectory, ignoredTmpName);
const ignoredHiddenName = '.gitignore';
const ignoredHiddenPath = path.join(subDirectory, ignoredHiddenName);

const watcher = fs.watch(tmpdir.path, {
  recursive: true,
  ignore: [
    '*.log',
    /\.tmp$/,
    (filename) => path.basename(filename).startsWith('.'),
  ],
});

watcher.on('change', common.mustCallAtLeast((event, filename) => {
  if (!filename) return;

  // On recursive watch, filename includes relative path from watched dir
  assert(!filename.endsWith(ignoredLogName));
  assert(!filename.endsWith(ignoredTmpName));
  assert(!filename.endsWith(ignoredHiddenName));

  if (filename.endsWith(testFileName)) {
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
