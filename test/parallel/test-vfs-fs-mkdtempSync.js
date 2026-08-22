// Flags: --experimental-vfs
'use strict';

// fs.mkdtempSync dispatches to VFS and returns a mount-rooted path, including
// the buffer-encoding variant.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
const mountPoint = myVfs.mount();

const prefix = path.join(mountPoint, 'src/tmp-');

// String result
{
  const dir = fs.mkdtempSync(prefix);
  assert.ok(dir.startsWith(prefix));
  assert.strictEqual(dir.length, prefix.length + 6);
  assert.strictEqual(fs.statSync(dir).isDirectory(), true);
}

// Buffer result
{
  const dir = fs.mkdtempSync(prefix, { encoding: 'buffer' });
  assert.ok(Buffer.isBuffer(dir));
}

myVfs.unmount();
