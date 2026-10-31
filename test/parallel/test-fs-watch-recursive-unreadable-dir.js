'use strict';

const common = require('../common');

// The recursive watcher must report directories it cannot look into through
// 'error' instead of throwing from inside its own fs.watch() callbacks, where
// nothing can catch the exception, and must keep working afterwards.

if (!common.isLinux)
  common.skip('this test exercises the inotify-based recursive watcher');

if (process.getuid() === 0)
  common.skip('as this test should not be run as `root`');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

const root = tmpdir.resolve('root');
const existing = path.join(root, 'existing');
const created = path.join(root, 'created');

// Registered before tmpdir.refresh() so that it runs before tmpdir's own
// 'exit' cleanup, which cannot remove a directory it is unable to read.
process.on('exit', () => {
  for (const dir of [existing, created]) {
    try {
      fs.chmodSync(dir, 0o755);
    } catch {
      // Never created.
    }
  }
});

tmpdir.refresh();
fs.mkdirSync(existing, { recursive: true });

// Resolves once `filename` is reported. Inotify reports events in order, so
// by then the events caused before it have been handled.
function reported(watcher, filename) {
  return new Promise((resolve) => {
    watcher.on('change', function onChange(eventType, name) {
      if (name === filename) {
        watcher.off('change', onChange);
        resolve();
      }
    });
  });
}

(async () => {
  {
    // A watched directory that becomes unreadable.
    const watcher = fs.watch(root, { recursive: true });
    watcher.on('error', common.mustCallAtLeast((error) => {
      assert.strictEqual(error.code, 'EACCES');
    }));
    const after = reported(watcher, 'after');
    fs.chmodSync(existing, 0o000);
    fs.writeFileSync(path.join(root, 'after'), '');
    await after;
    watcher.close();
    fs.chmodSync(existing, 0o755);
  }

  {
    // A directory that is already unreadable when its creation is handled is
    // picked up once it can be read.
    const watcher = fs.watch(root, { recursive: true });
    watcher.on('error', common.mustCallAtLeast((error) => {
      assert.strictEqual(error.code, 'EACCES');
      fs.chmodSync(created, 0o755);
    }));
    const inside = reported(watcher, path.join('created', 'inside'));
    fs.mkdirSync(created);
    fs.writeFileSync(path.join(created, 'inside'), '');
    fs.chmodSync(created, 0o000);
    await inside;
    watcher.close();
  }
})().then(common.mustCall());
