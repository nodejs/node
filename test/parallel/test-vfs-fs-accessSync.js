// Flags: --experimental-vfs
'use strict';

// fs.accessSync dispatches to VFS; missing paths throw ENOENT.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello');
const mountPoint = myVfs.mount();

// Existing path succeeds
fs.accessSync(path.join(mountPoint, 'src/hello.txt'));
fs.accessSync(path.join(mountPoint, 'src/hello.txt'), fs.constants.F_OK);

// Missing path throws ENOENT
assert.throws(() => fs.accessSync(path.join(mountPoint, 'missing')),
              { code: 'ENOENT' });

myVfs.unmount();
