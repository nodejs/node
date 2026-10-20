// Flags: --expose-internals
'use strict';

const common = require('../common');
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

const directory = tmpdir.resolve('retry-directory-disappears-before-read');
const child = path.join(directory, 'child');
const originalWatch = fs.watch;
const watcher = new FSWatcher({ recursive: true });
let childWatchCalls = 0;
let directoryListener;
let staleWatcherCloseCalls = 0;

fs.mkdirSync(child, { recursive: true });
fs.watch = common.mustCall((filename, options, listener) => {
  if (filename === directory) {
    directoryListener = listener;
    return createWatcher();
  }

  assert.strictEqual(filename, child);
  childWatchCalls++;
  if (childWatchCalls === 1) {
    fs.rmSync(child, { recursive: true });
    return { close: () => staleWatcherCloseCalls++ };
  }
  return createWatcher();
}, 3);

try {
  watcher[kFSWatchStart](directory);
  assert.strictEqual(childWatchCalls, 1);
  assert.strictEqual(staleWatcherCloseCalls, 1);

  fs.mkdirSync(child);
  directoryListener('rename', null);

  assert.strictEqual(childWatchCalls, 2);
} finally {
  watcher.close();
  fs.watch = originalWatch;
}
