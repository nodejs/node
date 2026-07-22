// Flags: --experimental-vfs
'use strict';

// Recursive directory watching: descendant changes trigger events.

const common = require('../common');
const { once } = require('events');
const vfs = require('node:vfs');

(async () => {
  // Recursive watch detects creation in a subdirectory
  {
    const myVfs = vfs.create();
    myVfs.mkdirSync('/d/sub', { recursive: true });
    myVfs.writeFileSync('/d/sub/a.txt', 'x');
    const watcher = myVfs.watch('/d', { interval: 25, recursive: true });
    watcher.on('change', common.mustCall(1)); // Making sure the event listener is called only once
    const changedPromise = once(watcher, 'change');
    myVfs.writeFileSync('/d/sub/b.txt', 'new');
    await changedPromise;
    watcher.close();
  }

  // Recursive watch detects modification of a pre-existing descendant
  {
    const myVfs = vfs.create();
    myVfs.mkdirSync('/r/sub', { recursive: true });
    myVfs.writeFileSync('/r/sub/file.txt', 'x');
    const watcher = myVfs.watch('/r', { interval: 25, recursive: true });
    watcher.on('change', common.mustCall(1)); // Making sure the event listener is called only once
    const changedPromise = once(watcher, 'change');
    myVfs.writeFileSync('/r/sub/file.txt', 'longer-content-changed');
    await changedPromise;
    watcher.close();
  }
})().then(common.mustCall());
