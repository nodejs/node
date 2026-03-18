'use strict';

// Tests for VFS readdir({ recursive: true }):
// - Returns deep entries as path strings
// - Returns deep entries as Dirent objects with correct types

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// readdir({ recursive: true }) returns deep entries.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/a.txt', 'a');
  myVfs.mkdirSync('/dir', { recursive: true });
  myVfs.writeFileSync('/dir/b.txt', 'b');
  myVfs.mount('/mnt');

  const entries = fs.readdirSync('/mnt', { recursive: true }).sort();
  assert.deepStrictEqual(entries, ['a.txt', 'dir', 'dir/b.txt']);

  myVfs.unmount();
}

// readdir({ recursive: true, withFileTypes: true }) returns Dirents.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/a.txt', 'a');
  myVfs.mkdirSync('/sub');
  myVfs.writeFileSync('/sub/b.txt', 'b');
  myVfs.mount('/mnt');

  const entries = fs.readdirSync('/mnt', {
    recursive: true,
    withFileTypes: true,
  });
  const names = entries.map((e) => e.name).sort();
  assert.deepStrictEqual(names, ['a.txt', 'sub', 'sub/b.txt']);

  // Verify types
  const fileEntry = entries.find((e) => e.name === 'a.txt');
  assert.strictEqual(fileEntry.isFile(), true);
  const dirEntry = entries.find((e) => e.name === 'sub');
  assert.strictEqual(dirEntry.isDirectory(), true);

  myVfs.unmount();
}
