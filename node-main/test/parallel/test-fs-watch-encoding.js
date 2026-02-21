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

// fs-watch on folders have limited capability in AIX.
// The testcase makes use of folder watching, and causes
// hang. This behavior is documented. Skip this for AIX.

if (common.isAIX)
  common.skip('folder watch capability is limited in AIX.');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const fn = '新建文夹件.txt';
const a = tmpdir.resolve(fn);

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

{
  // Test that using the `encoding` option has the expected result.
  const watcher = fs.watch(
    tmpdir.path,
    { encoding: 'hex' },
    (event, filename) => {
      if (['e696b0e5bbbae69687e5a4b9e4bbb62e747874', null].includes(filename))
        done(watcher);
    }
  );
  registerWatcher(watcher);
}

{
  // Test that in absence of `encoding` option has the expected result.
  const watcher = fs.watch(
    tmpdir.path,
    (event, filename) => {
      if ([fn, null].includes(filename))
        done(watcher);
    }
  );
  registerWatcher(watcher);
}

{
  // Test that using the `encoding` option has the expected result.
  const watcher = fs.watch(
    tmpdir.path,
    { encoding: 'buffer' },
    (event, filename) => {
      if (filename instanceof Buffer && filename.toString('utf8') === fn)
        done(watcher);
      else if (filename === null)
        done(watcher);
    }
  );
  registerWatcher(watcher);
}

const done = common.mustCall(unregisterWatcher, watchers.size);

// OS X and perhaps other systems can have surprising race conditions with
// file events. So repeat the operation in case it is missed the first time.
const interval = setInterval(() => {
  const fd = fs.openSync(a, 'w+');
  fs.closeSync(fd);
  fs.unlinkSync(a);
}, common.platformTimeout(100));
