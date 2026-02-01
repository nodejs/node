'use strict';

const common = require('../common');
const { skipIfNoWatch } = require('../common/watch.js');

skipIfNoWatch();

const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const subDirectory = path.join(tmpdir.path, 'subdir');
fs.mkdirSync(subDirectory);

const testFileName = 'file.txt';
const testFilePath = path.join(subDirectory, testFileName);
const ignoredFileName = 'file.log';
const ignoredFilePath = path.join(subDirectory, ignoredFileName);

const watcher = fs.watch(tmpdir.path, {
  recursive: true,
  ignore: '*.log',
});

watcher.on('change', common.mustCallAtLeast((event, filename) => {
  if (!filename) return;

  // On recursive watch, filename includes relative path from watched dir
  assert(!filename.endsWith(ignoredFileName));

  if (filename.endsWith(testFileName)) {
    watcher.close();
  }
}, 1));

function writeFiles() {
  fs.writeFileSync(ignoredFilePath, 'ignored');
  fs.writeFileSync(testFilePath, 'content');
}

if (common.isMacOS) {
  // Do the write with a delay to ensure that the OS is ready to notify us. See
  // https://github.com/nodejs/node/issues/52601.
  setTimeout(writeFiles, common.platformTimeout(100));
} else {
  writeFiles();
}
