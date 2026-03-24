'use strict';

// Tests for VFS directory watching:
// - watch() on directories reports child changes
// - Recursive watchers discover descendants created after startup

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// Modifying a child file of a watched directory must emit a change event.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/parent', { recursive: true });
  myVfs.writeFileSync('/parent/file.txt', 'x');

  const watcher = myVfs.watch('/parent', {
    interval: 50,
    persistent: false,
  }, common.mustCall((eventType, filename) => {
    assert.strictEqual(filename, 'file.txt');
    watcher.close();
  }));

  setTimeout(() => myVfs.writeFileSync('/parent/file.txt', 'y'), 100);
}

// Files created after a recursive watcher starts must still trigger events.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/parent', { recursive: true });

  let gotEvent = false;
  const watcher = myVfs.watch('/parent', {
    recursive: true,
    interval: 50,
    persistent: false,
  });
  watcher.on('change', common.mustCallAtLeast((eventType, filename) => {
    if (filename === 'new.txt') {
      gotEvent = true;
    }
  }));

  setTimeout(() => myVfs.writeFileSync('/parent/new.txt', 'first'), 70);
  setTimeout(common.mustCall(() => {
    watcher.close();
    assert.strictEqual(gotEvent, true);
  }), 300);
}
