// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const { kFSWatchStart } = require('internal/fs/watchers');
const { FSWatcher } = require('internal/fs/recursive_watch');

function createWatcher() {
  return { close() {} };
}

tmpdir.refresh();

const directory = tmpdir.resolve('watch-before-read');
const originalReaddirSync = fs.readdirSync;
const originalWatch = fs.watch;
const operations = [];
const watcher = new FSWatcher({ recursive: true });

fs.mkdirSync(directory);
fs.watch = common.mustCall((filename) => {
  assert.strictEqual(filename, directory);
  operations.push('watch');
  return createWatcher();
});
fs.readdirSync = common.mustCall((filename, ...args) => {
  assert.strictEqual(filename, directory);
  operations.push('read');
  return Reflect.apply(originalReaddirSync, fs, [filename, ...args]);
});

try {
  watcher[kFSWatchStart](directory);
  assert.deepStrictEqual(operations, ['watch', 'read']);
} finally {
  watcher.close();
  fs.readdirSync = originalReaddirSync;
  fs.watch = originalWatch;
}
