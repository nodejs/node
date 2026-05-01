'use strict';

// Cover VFSWatchAsyncIterable: promise-based watch().

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'a');

// Basic async iter — receive at least one change event
(async () => {
  const iter = myVfs.promises.watch('/file.txt', { interval: 25 });
  setTimeout(() => myVfs.writeFileSync('/file.txt', 'changed'), 60);
  for await (const evt of iter) {
    assert.strictEqual(evt.eventType, 'change');
    break; // closes via return()
  }
})().then(common.mustCall());

// Pre-aborted signal -> resolves immediately as done
(async () => {
  const ac = new AbortController();
  ac.abort();
  const iter = myVfs.promises.watch('/file.txt', { signal: ac.signal });
  const r = await iter.next();
  assert.strictEqual(r.done, true);
})().then(common.mustCall());

// Abort mid-flight -> rejects pending next() with AbortError
(async () => {
  const ac = new AbortController();
  const iter = myVfs.promises.watch('/file.txt', {
    signal: ac.signal,
    interval: 1000,
  });
  const pending = iter.next();
  setTimeout(() => ac.abort(), 20);
  try {
    await pending;
    throw new Error('Expected rejection');
  } catch (err) {
    assert.strictEqual(err.name, 'AbortError');
  }
})().then(common.mustCall());

// throw() on the iterator closes the watcher
(async () => {
  const iter = myVfs.promises.watch('/file.txt', { interval: 1000 });
  const r = await iter.throw(new Error('go away'));
  assert.strictEqual(r.done, true);
})().then(common.mustCall());

// Sync watch() also covers the basic flow
{
  const myVfs2 = vfs.create();
  myVfs2.writeFileSync('/file.txt', 'a');
  const watcher = myVfs2.watch('/file.txt', { interval: 25 },
                               common.mustCallAtLeast(() => {}, 1));
  setTimeout(() => {
    myVfs2.writeFileSync('/file.txt', 'b');
    setTimeout(() => watcher.close(), 100);
  }, 30);
}

// Recursive directory watch
{
  const myVfs3 = vfs.create();
  myVfs3.mkdirSync('/d/sub', { recursive: true });
  myVfs3.writeFileSync('/d/sub/file.txt', 'x');
  const watcher = myVfs3.watch('/d', { interval: 25, recursive: true },
                               common.mustCallAtLeast(() => {}, 1));
  setTimeout(() => {
    myVfs3.writeFileSync('/d/sub/file.txt', 'changed');
    setTimeout(() => watcher.close(), 100);
  }, 30);
}

// Buffer encoding
{
  const myVfs4 = vfs.create();
  myVfs4.writeFileSync('/file.txt', 'a');
  const watcher = myVfs4.watch('/file.txt', { interval: 25, encoding: 'buffer' },
                               common.mustCallAtLeast((eventType, filename) => {
                                 assert.strictEqual(eventType, 'change');
                                 assert.ok(Buffer.isBuffer(filename) || filename === null);
                               }, 1));
  setTimeout(() => {
    myVfs4.writeFileSync('/file.txt', 'b');
    setTimeout(() => watcher.close(), 100);
  }, 30);
}
