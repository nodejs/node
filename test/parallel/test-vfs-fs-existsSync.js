// Flags: --experimental-vfs
'use strict';

// fs.existsSync dispatches to VFS for paths under a mount.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello');
const mountPoint = myVfs.mount();

assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src/hello.txt')), true);
assert.strictEqual(fs.existsSync(path.join(mountPoint, 'src')), true);
assert.strictEqual(fs.existsSync(path.join(mountPoint, 'missing')), false);

myVfs.unmount();
