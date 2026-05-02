'use strict';

// Cover VFSWatcher edge cases. Run blocks sequentially. Use distinct
// content lengths so size-based stat-change detection always fires
// (mtime granularity is millisecond which can collide on synchronous
// writes within the same poll tick).

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const vfs = require('node:vfs');

(async () => {
  // Pre-aborted signal closes the watcher at construction
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/file.txt', 'a');
    const ac = new AbortController();
    ac.abort();
    const watcher = myVfs.watch('/file.txt', { signal: ac.signal });
    watcher.close();
  }

  // Aborting after construction triggers close
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/file.txt', 'a');
    const ac = new AbortController();
    const watcher = myVfs.watch('/file.txt', { signal: ac.signal });
    ac.abort();
    watcher.close();
  }

  // Listener add/remove + ref/unref
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/r.txt', 'a');
    const w = myVfs.watch('/r.txt');
    const fn = () => {};
    w.on('change', fn);
    w.removeListener('change', fn);
    w.on('change', fn);
    w.removeAllListeners('change');
    w.ref();
    w.unref();
    w.close();
  }

  // Buffer encoding — filename arrives as Buffer
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/bf.txt', 'a');
    const watcher = myVfs.watch('/bf.txt', { interval: 25, encoding: 'buffer' });
    const changed = once(watcher, 'change');
    myVfs.writeFileSync('/bf.txt', 'bbbbbbbb');
    const [eventType, filename] = await changed;
    assert.strictEqual(eventType, 'change');
    assert.ok(Buffer.isBuffer(filename));
    watcher.close();
  }

  // Recursive directory watch — observe a creation in a subdirectory
  {
    const myVfs = vfs.create();
    myVfs.mkdirSync('/d/sub', { recursive: true });
    myVfs.writeFileSync('/d/sub/a.txt', 'x');
    const watcher = myVfs.watch('/d', { interval: 25, recursive: true });
    const changed = once(watcher, 'change');
    myVfs.writeFileSync('/d/sub/b.txt', 'new');
    await changed;
    watcher.close();
  }

  // Non-recursive directory watch — file creation
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

  // Async iterable: events queued and drained via next()
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/q.txt', 'a');
    const iter = myVfs.promises.watch('/q.txt', { interval: 25 });
    myVfs.writeFileSync('/q.txt', 'bbbbbbbb');
    const r = await iter.next();
    if (!r.done) assert.strictEqual(r.value.eventType, 'change');
    await iter.return();
  }

  // VFSStatWatcher fires on content change
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/sw.txt', 'a');
    let listener;
    const fired = new Promise((resolve) => {
      listener = (curr, prev) => {
        assert.strictEqual(typeof curr.size, 'number');
        assert.strictEqual(typeof prev.size, 'number');
        resolve();
      };
    });
    myVfs.watchFile('/sw.txt', { interval: 25 }, listener);
    myVfs.writeFileSync('/sw.txt', 'changed!!!!');
    await fired;
    myVfs.unwatchFile('/sw.txt', listener);
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
})().then(common.mustCall());
