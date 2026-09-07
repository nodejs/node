// Flags: --experimental-vfs
'use strict';

// fs.watch on a path under a mount returns the provider's watcher object
// rather than calling the real-fs watcher.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello');
const mountPoint = myVfs.mount();

const watcher = fs.watch(path.join(mountPoint, 'src/hello.txt'));
assert.ok(watcher);
assert.strictEqual(typeof watcher.close, 'function');
watcher.close();

myVfs.unmount();
