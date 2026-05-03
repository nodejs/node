'use strict';

// Branch coverage for VFSWatcher / VFSStatWatcher / VFSWatchAsyncIterable.

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const vfs = require('node:vfs');

(async () => {
  // close() while a poll is in-flight after #closed flag is set —
  // close + close again is the simplest #closed-true branch.
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/x.txt', 'a');
    const watcher = myVfs.watch('/x.txt');
    watcher.close();
    watcher.close(); // second close is a no-op (#closed already true)
  }

  // Persistent: false reaches the unref branch.
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/p.txt', 'a');
    const watcher = myVfs.watch('/p.txt', { persistent: false });
    watcher.close();
  }

  // Watching a directory and deleting a tracked file (covers the
  // `file deleted` path in #pollDirectory).
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

  // Watching a directory whose listing fails mid-poll: delete the
  // directory itself to trigger the `try/catch { /* ignore */ }`
  // around readdirSync inside #pollDirectory.
  {
    const myVfs = vfs.create();
    myVfs.mkdirSync('/gone');
    myVfs.writeFileSync('/gone/f.txt', 'x');
    const watcher = myVfs.watch('/gone', { interval: 25 });
    myVfs.rmSync('/gone', { recursive: true });
    // give the poll one tick
    await new Promise((r) => setTimeout(r, 60));
    watcher.close();
  }

  // VFSStatWatcher with bigint option — covers ctime/size branches and
  // the bigint createZeroStats path.
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/sw.txt', 'a');
    let listener;
    const fired = new Promise((resolve) => {
      listener = (curr, prev) => {
        assert.strictEqual(typeof curr.size, 'bigint');
        assert.strictEqual(typeof prev.size, 'bigint');
        resolve();
      };
    });
    myVfs.watchFile('/sw.txt', { interval: 25, bigint: true }, listener);
    myVfs.writeFileSync('/sw.txt', 'changed!!!!');
    await fired;
    myVfs.unwatchFile('/sw.txt', listener);
  }

  // VFSStatWatcher default interval (no interval option provided)
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/dw.txt', 'a');
    const listener = () => {};
    myVfs.watchFile('/dw.txt', listener);
    myVfs.unwatchFile('/dw.txt', listener);
  }

  // VFSStatWatcher: stop on already-stopped watcher is a no-op
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/sw2.txt', 'a');
    const listener = () => {};
    myVfs.watchFile('/sw2.txt', { interval: 25 }, listener);
    myVfs.unwatchFile('/sw2.txt', listener);
    // unwatch again
    myVfs.unwatchFile('/sw2.txt', listener);
  }

  // Async iterable: emit a change while a next() is outstanding (covers the
  // pendingResolvers shift path)
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/q.txt', 'a');
    const iter = myVfs.promises.watch('/q.txt', { interval: 25 });
    const pending = iter.next();
    myVfs.writeFileSync('/q.txt', 'BBBBBBBB');
    const r = await pending;
    if (!r.done) assert.strictEqual(r.value.eventType, 'change');
    await iter.return();
  }

  // Async iterable throw() closes the watcher and resolves with done:true
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/q2.txt', 'a');
    const iter = myVfs.promises.watch('/q2.txt', { interval: 1000 });
    const r = await iter.throw(new Error('boom'));
    assert.strictEqual(r.done, true);
  }

  // Async iterable: queue-fill path — keep modifying without consuming.
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/q3.txt', 'a');
    const iter = myVfs.promises.watch('/q3.txt', { interval: 25 });
    for (let i = 0; i < 5; i++) {
      myVfs.writeFileSync('/q3.txt', 'x'.repeat(i + 5));
      await new Promise((r) => setTimeout(r, 30));
    }
    // Drain at least one event
    const r = await iter.next();
    assert.ok(r.value || r.done);
    await iter.return();
  }

  // Async iterable: close while a resolver is pending — drains via the
  // 'close' event handler (covers the close-event resolver-loop branch).
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/q4.txt', 'a');
    const iter = myVfs.promises.watch('/q4.txt', { interval: 1000 });
    const pending = iter.next();
    // Queue iter.return() on a microtask so it runs before pending resolves
    queueMicrotask(() => iter.return());
    const r = await pending;
    assert.strictEqual(r.done, true);
  }
})().then(common.mustCall());
