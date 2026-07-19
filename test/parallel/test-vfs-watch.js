// Flags: --experimental-vfs
'use strict';

// Tests for VFS watch() on a single file: change detection, listener
// registration, ref/unref, and the watch-then-create flow.

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const vfs = require('node:vfs');

(async () => {
  // Listener as 2nd argument
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/lf.txt', 'a');
    const w = myVfs.watch('/lf.txt', common.mustNotCall());
    w.close();
  }

  // Listener add/remove + ref/unref smoke
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/r.txt', 'a');
    const w = myVfs.watch('/r.txt');
    const fn = common.mustNotCall();
    w.on('change', fn);
    w.removeListener('change', fn);
    w.on('change', fn);
    w.removeAllListeners('change');
    w.ref();
    w.unref();
    w.close();
  }

  // Double close is a no-op
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/x.txt', 'a');
    const watcher = myVfs.watch('/x.txt');
    watcher.close();
    watcher.close();
  }

  // persistent: false reaches the unref branch
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/p.txt', 'a');
    const watcher = myVfs.watch('/p.txt', { persistent: false });
    watcher.close();
  }

  // Watching a missing path then creating it
  {
    const myVfs = vfs.create();
    const watcher = myVfs.watch('/late.txt', { interval: 25 });
    const changed = once(watcher, 'change');
    myVfs.writeFileSync('/late.txt', 'now');
    await changed;
    watcher.close();
  }

  // Modifying the watched file emits change with the basename as filename
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/single.txt', 'a');
    const watcher = myVfs.watch('/single.txt', { interval: 25 });
    const evt = once(watcher, 'change');
    myVfs.writeFileSync('/single.txt', 'longer-content-changed');
    const [eventType, filename] = await evt;
    assert.strictEqual(eventType, 'change');
    assert.strictEqual(filename, 'single.txt');
    watcher.close();
  }
})().then(common.mustCall());
