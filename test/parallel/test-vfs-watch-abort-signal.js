// Flags: --experimental-vfs
'use strict';

// AbortSignal handling for watch() and promises.watch().

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

(async () => {
  // Pre-aborted signal closes the watcher at construction
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/file.txt', 'a');
    const watcher = myVfs.watch('/file.txt', { signal: AbortSignal.abort() });
    watcher.on('change', common.mustNotCall());
    setImmediate(() => myVfs.writeFileSync('/file.txt', 'b'));
  }

  // Aborting after construction triggers close
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/file.txt', 'a');
    const ac = new AbortController();
    const watcher = myVfs.watch('/file.txt', { signal: ac.signal });
    watcher.on('change', common.mustNotCall());
    ac.abort();
    setImmediate(() => myVfs.writeFileSync('/file.txt', 'b'));
  }

  // promises.watch with pre-aborted signal resolves done immediately
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/p.txt', 'a');
    const iter = myVfs.promises.watch('/p.txt', { signal: AbortSignal.abort() });
    const r = await iter.next();
    assert.strictEqual(r.done, true);
  }

  // promises.watch with mid-flight abort rejects pending next() with AbortError
  {
    const myVfs = vfs.create();
    myVfs.writeFileSync('/p2.txt', 'a');
    const ac = new AbortController();
    const iter = myVfs.promises.watch('/p2.txt', {
      signal: ac.signal,
      interval: 1000,
    });
    const pending = iter.next();
    queueMicrotask(() => ac.abort());
    await assert.rejects(pending, { name: 'AbortError' });
  }
})().then(common.mustCall());
