'use strict';

const common = require('../common');

// A symbolic link that cannot be watched (here, one of a pair pointing at
// each other) appearing in a watched tree must be reported through 'error'
// instead of being thrown from inside the recursive watcher's own fs.watch()
// callbacks, and the watcher must keep working.

if (!common.isLinux)
  common.skip('this test exercises the inotify-based recursive watcher');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const root = tmpdir.resolve('root');
fs.mkdirSync(root);

const watcher = fs.watch(root, { recursive: true });
watcher.on('error', common.mustCallAtLeast((error) => {
  assert.strictEqual(error.code, 'ELOOP');
}));
watcher.on('change', common.mustCallAtLeast((eventType, filename) => {
  // Inotify reports events in order, so by the time the file created last is
  // reported, the events about the links have been handled.
  if (filename === 'after') {
    watcher.close();
  }
}));

fs.symlinkSync(path.join(root, 'b'), path.join(root, 'a'));
fs.symlinkSync(path.join(root, 'a'), path.join(root, 'b'));
fs.writeFileSync(path.join(root, 'after'), '');
