// Flags: --experimental-vfs
'use strict';

// Tests for VFS directory watching:
// - watch() on directories reports child changes
// - File creation / deletion events
// - Listing failures during a poll are swallowed

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
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

(async () => {
  // Non-recursive directory watch: file creation
  {
    const myVfs = vfs.create();
    myVfs.mkdirSync('/d');
    myVfs.writeFileSync('/d/a.txt', 'x');
    const watcher = myVfs.watch('/d', { interval: 25 });
    const changed = once(watcher, 'change');
    myVfs.writeFileSync('/d/new.txt', 'x');
    await changed;
    watcher.close();
  }

  // Non-recursive directory watch: file deletion of a tracked child
  {
    const myVfs = vfs.create();
    myVfs.mkdirSync('/dd');
    myVfs.writeFileSync('/dd/keep.txt', 'a');
    myVfs.writeFileSync('/dd/goes.txt', 'b');
    const watcher = myVfs.watch('/dd', { interval: 25 });
    const evt = once(watcher, 'change');
    myVfs.unlinkSync('/dd/goes.txt');
    await evt;
    watcher.close();
  }

  // The watched directory is removed mid-poll: readdirSync inside the
  // poll throws and the watcher swallows the error.
  {
    const myVfs = vfs.create();
    myVfs.mkdirSync('/gone');
    myVfs.writeFileSync('/gone/f.txt', 'x');
    const watcher = myVfs.watch('/gone', { interval: 25 });
    myVfs.rmSync('/gone', { recursive: true });
    await new Promise((r) => setTimeout(r, 60));
    watcher.close();
  }
})().then(common.mustCall());
