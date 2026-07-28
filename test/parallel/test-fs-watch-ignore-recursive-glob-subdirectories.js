'use strict';

const common = require('../common');
const { skipIfNoWatch } = require('../common/watch.js');

skipIfNoWatch();

const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const nodeModules = path.join(tmpdir.path, 'node_modules');
const srcDir = path.join(tmpdir.path, 'src');

fs.mkdirSync(nodeModules);
fs.mkdirSync(srcDir);

const testFileName = 'app.js';
const testFilePath = path.join(srcDir, testFileName);
const ignoredFileName = 'package.json';
const ignoredFilePath = path.join(nodeModules, ignoredFileName);

const watcher = fs.watch(tmpdir.path, {
  recursive: true,
  // On Linux, matching the directory skips watching it entirely.
  // On macOS, the native watcher still needs to filter file events inside.
  ignore: ['**/node_modules/**', '**/node_modules'],
});

watcher.on('change', common.mustCallAtLeast((event, filename) => {
  if (!filename) return;

  // On recursive watch, filename includes relative path from watched dir
  assert(!filename.includes('node_modules'));

  if (filename.endsWith(testFileName)) {
    watcher.close();
  }
}, 1));

function writeFiles() {
  fs.writeFileSync(ignoredFilePath, '{}');
  fs.writeFileSync(testFilePath, 'console.log("hello-' + Date.now() + '")');
}

if (common.isMacOS) {
  // Do the write with a delay to ensure that the OS is ready to notify us. See
  // https://github.com/nodejs/node/issues/52601.
  setTimeout(writeFiles, common.platformTimeout(100));
} else {
  writeFiles();
}
