// Flags: --expose-internals
'use strict';

const common = require('../common');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const { kFSWatchStart } = require('internal/fs/watchers');
const { FSWatcher } = require('internal/fs/recursive_watch');

tmpdir.refresh();

const directory = tmpdir.resolve('root-disappears-before-watch');
const originalWatch = fs.watch;
const watcher = new FSWatcher({ recursive: true });
let watchError;

fs.mkdirSync(directory);
fs.watch = common.mustCall((filename, ...args) => {
  assert.strictEqual(filename, directory);
  fs.rmSync(directory, { recursive: true });
  try {
    return Reflect.apply(originalWatch, fs, [filename, ...args]);
  } catch (error) {
    watchError = error;
    throw error;
  }
});

try {
  assert.throws(
    () => watcher[kFSWatchStart](directory),
    (error) => {
      assert.strictEqual(error, watchError);
      assert.match(error.code, /^(ENOENT|ENODEV)$/);
      return true;
    },
  );
} finally {
  watcher.close();
  fs.watch = originalWatch;
}
