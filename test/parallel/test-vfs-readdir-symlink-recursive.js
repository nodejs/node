'use strict';

// Recursive readdir must follow symlinks to directories.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/real-dir');
myVfs.writeFileSync('/real-dir/nested.txt', 'nested');

myVfs.mkdirSync('/root');
myVfs.symlinkSync('/real-dir', '/root/symdir');

const entries = myVfs.readdirSync('/root', { recursive: true });
assert.ok(entries.includes('symdir'));
assert.ok(
  entries.includes('symdir/nested.txt'),
  `Expected 'symdir/nested.txt' in entries: ${entries}`,
);

// Recursive readdir with withFileTypes:true returns Dirent objects whose
// parentPath reflects the actual location of the entry (not the entry's
// stringified relative path).
{
  const v = vfs.create();
  v.mkdirSync('/r/a/b', { recursive: true });
  v.writeFileSync('/r/top.txt', 'x');
  v.writeFileSync('/r/a/b/leaf.txt', 'y');

  const dirents = v.readdirSync('/r', { withFileTypes: true, recursive: true });
  const leaf = dirents.find((d) => d.name === 'leaf.txt');
  assert.ok(leaf);
  assert.strictEqual(leaf.parentPath, '/r/a/b');

  const top = dirents.find((d) => d.name === 'top.txt');
  assert.ok(top);
  assert.strictEqual(top.parentPath, '/r');
}

// Non-recursive readdir with withFileTypes returns mixed entry types
{
  const v = vfs.create();
  v.mkdirSync('/d');
  v.writeFileSync('/d/a.txt', 'x');
  v.mkdirSync('/d/sub');
  v.symlinkSync('a.txt', '/d/lnk');
  const dirents = v.readdirSync('/d', { withFileTypes: true });
  assert.ok(dirents.find((d) => d.name === 'a.txt' && d.isFile()));
  assert.ok(dirents.find((d) => d.name === 'sub' && d.isDirectory()));
  assert.ok(dirents.find((d) => d.name === 'lnk' && d.isSymbolicLink()));
}
