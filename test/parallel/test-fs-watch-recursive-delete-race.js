// Flags: --expose-internals
'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { kFSWatchStart } = require('internal/fs/watchers');
const { FSWatcher } = require('internal/fs/recursive_watch');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

tmpdir.refresh();

const parent = tmpdir.resolve('parent');
const child = path.join(parent, 'child');
fs.mkdirSync(child, { recursive: true });
fs.writeFileSync(path.join(child, 'test.tmp'), 'test');

const watch = fs.watch;
let deletedChild = false;
fs.watch = function(filename, ...args) {
  const watcher = Reflect.apply(watch, this, [filename, ...args]);

  if (filename === child) {
    fs.rmSync(child, { recursive: true });
    deletedChild = true;
  }

  return watcher;
};

const watcher = new FSWatcher({ recursive: true });
watcher.on('error', common.mustNotCall());

try {
  watcher[kFSWatchStart](parent);
  assert.strictEqual(deletedChild, true);
} finally {
  watcher.close();
  fs.watch = watch;
}
