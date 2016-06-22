'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');
const assert = require('assert');

if (common.isFreeBSD) {
  common.skip('Test currently not working on FreeBSD');
  return;
}

common.refreshTmpDir();

const fn = '新建文夹件.txt';
const a = path.join(common.tmpDir, fn);

const watchers = new Set();

function registerWatcher(watcher) {
  watchers.add(watcher);
}

function unregisterWatcher(watcher) {
  watcher.close();
  watchers.delete(watcher);
  if (watchers.size === 0) {
    clearInterval(interval);
  }
}

const watcher1 = fs.watch(
  common.tmpDir,
  {encoding: 'hex'},
  (event, filename) => {
    console.log(event);
    assert.equal(filename, 'e696b0e5bbbae69687e5a4b9e4bbb62e747874');
    unregisterWatcher(watcher1);
  }
);
registerWatcher(watcher1);

const watcher2 = fs.watch(
  common.tmpDir,
  (event, filename) => {
    assert.equal(filename, fn);
    unregisterWatcher(watcher2);
  }
);
registerWatcher(watcher2);

const watcher3 = fs.watch(
  common.tmpDir,
  {encoding: 'buffer'},
  (event, filename) => {
    assert(filename instanceof Buffer);
    assert.equal(filename.toString('utf8'), fn);
    unregisterWatcher(watcher3);
  }
);
registerWatcher(watcher3);

// OS X and perhaps other systems can have surprising race conditions with
// file events. So repeat the operation in case it is missed the first time.
const interval = setInterval(() => {
  const fd = fs.openSync(a, 'w+');
  fs.closeSync(fd);
  fs.unlinkSync(a);
}, common.platformTimeout(100));
