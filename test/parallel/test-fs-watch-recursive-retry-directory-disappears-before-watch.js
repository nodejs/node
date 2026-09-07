// Flags: --expose-internals
'use strict';

const common = require('../common');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');
const { kFSWatchStart } = require('internal/fs/watchers');
const { FSWatcher } = require('internal/fs/recursive_watch');

function createWatcher() {
  return { close() {} };
}

tmpdir.refresh();

const directory = tmpdir.resolve('retry-directory-disappears-before-watch');
const child = path.join(directory, 'child');
const originalWatch = fs.watch;
const changes = [];
const watcher = new FSWatcher({ recursive: true });
let childWatchCalls = 0;
let directoryListener;
let watchError;

fs.mkdirSync(child, { recursive: true });
watcher.on('change', (eventType, filename) => changes.push([eventType, filename]));
fs.watch = common.mustCall((filename, options, listener) => {
  if (filename === directory) {
    directoryListener = listener;
    return createWatcher();
  }

  assert.strictEqual(filename, child);
  childWatchCalls++;
  if (childWatchCalls === 1) {
    fs.rmSync(child, { recursive: true });
    try {
      return Reflect.apply(originalWatch, fs, [filename, options, listener]);
    } catch (error) {
      watchError = error;
      throw error;
    }
  }
  return createWatcher();
}, 3);

try {
  watcher[kFSWatchStart](directory);
  assert.strictEqual(childWatchCalls, 1);
  assert.match(watchError?.code, /^(ENOENT|ENODEV)$/);

  fs.mkdirSync(child);
  directoryListener('rename', null);

  assert.strictEqual(childWatchCalls, 2);
  assert.deepStrictEqual(changes, [['rename', 'child']]);
} finally {
  watcher.close();
  fs.watch = originalWatch;
}
