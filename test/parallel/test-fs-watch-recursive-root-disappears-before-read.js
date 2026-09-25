// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const { kFSWatchStart } = require('internal/fs/watchers');
const { FSWatcher } = require('internal/fs/recursive_watch');

tmpdir.refresh();

const directory = tmpdir.resolve('root-disappears-before-read');
const originalWatch = fs.watch;
const watcher = new FSWatcher({ recursive: true });
let closeCalls = 0;

fs.mkdirSync(directory);
fs.watch = common.mustCall((filename) => {
  assert.strictEqual(filename, directory);
  fs.rmSync(directory, { recursive: true });
  return { close: () => closeCalls++ };
});

try {
  assert.throws(
    () => watcher[kFSWatchStart](directory),
    {
      code: 'ENOENT',
      filename: directory,
    },
  );
  assert.strictEqual(closeCalls, 1);
} finally {
  watcher.close();
  fs.watch = originalWatch;
}
