'use strict';

// This test is a bit more complicated than it ideally needs to be to work
// around issues on OS X and SmartOS.
//
// On OS X, watch events are subject to peculiar timing oddities such that an
// event might fire out of order. The synchronous refreshing of the tmp
// directory might trigger an event on the watchers that are instantiated after
// it!
//
// On SmartOS, the watch events fire but the filename is null.

const common = require('../common');
const fs = require('fs');
const path = require('path');

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
    if (['e696b0e5bbbae69687e5a4b9e4bbb62e747874', null].includes(filename))
      done(watcher1);
  }
);
registerWatcher(watcher1);

const watcher2 = fs.watch(
  common.tmpDir,
  (event, filename) => {
    if ([fn, null].includes(filename))
      done(watcher2);
  }
);
registerWatcher(watcher2);

const watcher3 = fs.watch(
  common.tmpDir,
  {encoding: 'buffer'},
  (event, filename) => {
    if (filename instanceof Buffer && filename.toString('utf8') === fn)
      done(watcher3);
    else if (filename === null)
      done(watcher3);
  }
);
registerWatcher(watcher3);

const done = common.mustCall(unregisterWatcher, watchers.size);

// OS X and perhaps other systems can have surprising race conditions with
// file events. So repeat the operation in case it is missed the first time.
const interval = setInterval(() => {
  const fd = fs.openSync(a, 'w+');
  fs.closeSync(fd);
  fs.unlinkSync(a);
}, common.platformTimeout(100));
