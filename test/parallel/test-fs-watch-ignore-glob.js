'use strict';

const common = require('../common');
const { skipIfNoWatch } = require('../common/watch.js');

skipIfNoWatch();

const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const testFileName = 'file.txt';
const testFilePath = path.join(tmpdir.path, testFileName);
const ignoredFileName = 'file.log';
const ignoredFilePath = path.join(tmpdir.path, ignoredFileName);

const watcher = fs.watch(tmpdir.path, {
  ignore: '*.log',
});

watcher.on('change', common.mustCallAtLeast((event, filename) => {
  assert.notStrictEqual(filename, ignoredFileName);

  if (filename === testFileName) {
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
