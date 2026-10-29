// Flags: --expose-internals
'use strict';

const common = require('../common');

if (!common.isAIX)
  common.skip('AIX-specific ENODEV handling');

const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const { kFSWatchStart } = require('internal/fs/watchers');
const { FSWatcher } = require('internal/fs/recursive_watch');

tmpdir.refresh();

const directory = tmpdir.resolve('preserve-watch-error');
const originalStatSync = fs.statSync;
const originalWatch = fs.watch;
const expected = new Error('watch failed');
const verificationError = new Error('stat failed');
const watcher = new FSWatcher({ recursive: true, throwIfNoEntry: false });
const failVerification = common.mustCall(() => {
  throw verificationError;
});

expected.code = 'ENODEV';
verificationError.code = 'EIO';
fs.mkdirSync(directory);
fs.watch = common.mustCall((filename) => {
  assert.strictEqual(filename, directory);
  throw expected;
});
fs.statSync = (filename, options) => {
  if (filename === directory && options?.throwIfNoEntry === false) {
    return failVerification();
  }
  return Reflect.apply(originalStatSync, fs, [filename, options]);
};

try {
  assert.throws(
    () => watcher[kFSWatchStart](directory),
    (error) => {
      assert.strictEqual(error, expected);
      return true;
    },
  );
} finally {
  watcher.close();
  fs.statSync = originalStatSync;
  fs.watch = originalWatch;
}
