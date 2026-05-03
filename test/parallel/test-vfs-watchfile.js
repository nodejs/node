'use strict';

// Tests for VFS watchFile/unwatchFile.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// unwatchFile(path) without a specific listener cleans up the timer.
// If the timer leaks, the process would hang.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/a.txt', 'x');
  myVfs.watchFile('/a.txt', { interval: 50, persistent: false }, () => {});
  myVfs.unwatchFile('/a.txt');
}

// Default options: no interval option provided
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/dw.txt', 'a');
  const listener = () => {};
  myVfs.watchFile('/dw.txt', listener);
  myVfs.unwatchFile('/dw.txt', listener);
}

// Listener as 2nd argument (no options object)
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/lf.txt', 'a');
  const listener = () => {};
  myVfs.watchFile('/lf.txt', listener);
  myVfs.unwatchFile('/lf.txt', listener);
}

// Double unwatch is a no-op
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/sw.txt', 'a');
  const listener = () => {};
  myVfs.watchFile('/sw.txt', { interval: 25 }, listener);
  myVfs.unwatchFile('/sw.txt', listener);
  myVfs.unwatchFile('/sw.txt', listener);
}

// Zero stats for a missing file: prev.isFile() is false and prev.mode is 0
{
  const myVfs = vfs.create();
  function listener(curr, prev) {
    assert.strictEqual(prev.isFile(), false);
    assert.strictEqual(prev.mode, 0);
    myVfs.unwatchFile('/missing.txt', listener);
  }
  myVfs.watchFile('/missing.txt', { interval: 50, persistent: false }, listener);
  setTimeout(() => myVfs.writeFileSync('/missing.txt', 'x'), 100);
}

// Content change fires the listener with curr/prev stats
(async () => {
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
  myVfs.writeFileSync('/sw.txt', 'longer-content-changed');
  await fired;
  myVfs.unwatchFile('/sw.txt', listener);
})().then(common.mustCall());

// bigint: true returns BigInt fields in both curr and prev stats, plus
// the bigint createZeroStats path when watching an initially-missing file.
(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/bi.txt', 'a');
  let listener;
  const fired = new Promise((resolve) => {
    listener = (curr, prev) => {
      assert.strictEqual(typeof curr.size, 'bigint');
      assert.strictEqual(typeof prev.size, 'bigint');
      resolve();
    };
  });
  myVfs.watchFile('/bi.txt', { interval: 25, bigint: true }, listener);
  myVfs.writeFileSync('/bi.txt', 'longer-content-changed');
  await fired;
  myVfs.unwatchFile('/bi.txt', listener);
})().then(common.mustCall());

// bigint: true on a missing file emits BigInt prev.size = 0n
{
  const myVfs = vfs.create();
  const watcher = myVfs.watchFile('/missing-b.txt',
                                  { interval: 50, persistent: false, bigint: true },
                                  common.mustCallAtLeast((curr, prev) => {
                                    assert.strictEqual(typeof curr.size, 'bigint');
                                    assert.strictEqual(typeof prev.size, 'bigint');
                                    myVfs.unwatchFile('/missing-b.txt');
                                  }, 1));
  setTimeout(() => myVfs.writeFileSync('/missing-b.txt', 'now-here'), 80);
  setTimeout(() => myVfs.unwatchFile('/missing-b.txt'), 500);
  if (watcher && watcher.unref) watcher.unref();
}
