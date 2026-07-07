// Flags: --experimental-vfs
'use strict';

// fs.readdirSync dispatches to VFS, including the buffer-encoding and
// withFileTypes options.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/src/subdir', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello');
myVfs.writeFileSync('/src/data.json', '{}');
const mountPoint = myVfs.mount();

// Default (utf8 string array)
{
  const entries = fs.readdirSync(path.join(mountPoint, 'src'));
  assert.ok(entries.includes('hello.txt'));
  assert.ok(entries.includes('data.json'));
  assert.ok(entries.includes('subdir'));
}

// withFileTypes: true -> Dirent array
{
  const dirents = fs.readdirSync(path.join(mountPoint, 'src'),
                                 { withFileTypes: true });
  const hello = dirents.find((d) => d.name === 'hello.txt');
  assert.ok(hello);
  assert.strictEqual(hello.isFile(), true);
  const subdir = dirents.find((d) => d.name === 'subdir');
  assert.strictEqual(subdir.isDirectory(), true);
}

// encoding: 'buffer' -> Buffer entries
{
  const entries = fs.readdirSync(path.join(mountPoint, 'src'),
                                 { encoding: 'buffer' });
  assert.ok(entries.every(Buffer.isBuffer));
  assert.ok(entries.some((b) => b.toString() === 'hello.txt'));
}

myVfs.unmount();
