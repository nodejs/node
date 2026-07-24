// Flags: --experimental-vfs
'use strict';

// fs.realpathSync dispatches to VFS and returns a mount-rooted absolute path.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello');
const mountPoint = myVfs.mount();

const p = path.join(mountPoint, 'src/hello.txt');

// Default string return
assert.strictEqual(fs.realpathSync(p), p);

// Buffer encoding
{
  const rp = fs.realpathSync(p, { encoding: 'buffer' });
  assert.ok(Buffer.isBuffer(rp));
  assert.strictEqual(rp.toString(), p);
}

myVfs.unmount();
