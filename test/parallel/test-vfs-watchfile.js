// Flags: --experimental-vfs
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
  myVfs.watchFile('/a.txt', { interval: 50, persistent: false }, common.mustNotCall());
  myVfs.unwatchFile('/a.txt');
}

// Default options: no interval option provided
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/dw.txt', 'a');
  const listener = common.mustNotCall();
  myVfs.watchFile('/dw.txt', listener);
  myVfs.unwatchFile('/dw.txt', listener);
  myVfs.writeFileSync('/dw.txt', 'b');
}

// Double unwatch is a no-op
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/sw.txt', 'a');
  const listener = common.mustNotCall();
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
  myVfs.watchFile('/missing.txt', { interval: 50, persistent: false }, common.mustCall(listener));
  setImmediate(() => myVfs.writeFileSync('/missing.txt', 'x'));
}

// Content change fires the listener with curr/prev stats
(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/sw.txt', 'a');
  const listener = common.mustCallAtLeast((curr, prev) => {
    assert.strictEqual(typeof curr.size, 'number');
    assert.strictEqual(typeof prev.size, 'number');
  }, 1);
  const fired = new Promise((resolve) => {
    myVfs.watchFile('/sw.txt', { interval: 25 }, (curr, prev) => {
      listener(curr, prev);
      resolve();
    });
  });
  myVfs.writeFileSync('/sw.txt', 'longer-content-changed');
  await fired;
  myVfs.unwatchFile('/sw.txt');
})().then(common.mustCall());

// bigint: true returns BigInt fields in both curr and prev stats, plus
// the bigint createZeroStats path when watching an initially-missing file.
(async () => {
  const myVfs = vfs.create();
  myVfs.writeFileSync('/bi.txt', 'a');
  const listener = common.mustCallAtLeast((curr, prev) => {
    assert.strictEqual(typeof curr.size, 'bigint');
    assert.strictEqual(typeof prev.size, 'bigint');
  }, 1);
  const fired = new Promise((resolve) => {
    myVfs.watchFile('/bi.txt', { interval: 25, bigint: true }, (curr, prev) => {
      listener(curr, prev);
      resolve();
    });
  });
  myVfs.writeFileSync('/bi.txt', 'longer-content-changed');
  await fired;
  myVfs.unwatchFile('/bi.txt');
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
  if (watcher?.unref) watcher.unref();
}
