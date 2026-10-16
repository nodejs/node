'use strict';

const common = require('../common');

// Events naming entries of a watched directory that has meanwhile been
// replaced by a file must not make the recursive watcher throw from inside
// its own fs.watch() callbacks: those entries are simply gone.

if (!common.isLinux)
  common.skip('this test exercises the inotify-based recursive watcher');

const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const root = tmpdir.resolve('root');
const sub = path.join(root, 'sub');
fs.mkdirSync(sub, { recursive: true });

const watcher = fs.watch(root, { recursive: true });
watcher.on('error', common.mustNotCall());
watcher.on('change', common.mustCallAtLeast((eventType, filename) => {
  // Inotify reports events in order, so by the time the file created last is
  // reported, the events naming entries of `sub` have been handled.
  if (filename === 'after') {
    watcher.close();
  }
}));

fs.writeFileSync(path.join(sub, 'file'), '');
fs.rmSync(sub, { recursive: true });
fs.writeFileSync(sub, '');
fs.writeFileSync(path.join(root, 'after'), '');
