// Flags: --experimental-vfs
'use strict';

// Recursive readdir must follow symlinks to directories.

const common = require('../common');
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
  assert.ok(dirents.some((d) => d.name === 'a.txt' && d.isFile()));
  assert.ok(dirents.some((d) => d.name === 'sub' && d.isDirectory()));
  assert.ok(dirents.some((d) => d.name === 'lnk' && d.isSymbolicLink()));
}

// Recursive readdir on circular symlinks must terminate, not overflow the
// stack. Regression test for https://github.com/nodejs/node/issues/64148

// Self-referential symlink: /dir/loop -> /dir
{
  const v = vfs.create();
  v.mkdirSync('/dir');
  v.symlinkSync('/dir', '/dir/loop');

  const entries = v.readdirSync('/', { recursive: true });
  // Terminates, follows the symlink at least one level, stays bounded.
  assert.ok(entries.includes('dir'));
  assert.ok(entries.includes('dir/loop'));
  assert.ok(entries.includes('dir/loop/loop'));
  assert.ok(entries.length < 100, `unbounded result: ${entries.length}`);
}

// Mutual circular chain: /a/link_to_b -> /b and /b/link_to_a -> /a
{
  const v = vfs.create();
  v.mkdirSync('/a');
  v.mkdirSync('/b');
  v.symlinkSync('/a', '/b/link_to_a');
  v.symlinkSync('/b', '/a/link_to_b');

  const entries = v.readdirSync('/', { recursive: true });
  assert.ok(entries.includes('a'));
  assert.ok(entries.includes('b'));
  assert.ok(entries.length < 200, `unbounded result: ${entries.length}`);
}

// withFileTypes:true on a circular symlink must also terminate.
{
  const v = vfs.create();
  v.mkdirSync('/dir');
  v.symlinkSync('/dir', '/dir/loop');

  const dirents = v.readdirSync('/', { withFileTypes: true, recursive: true });
  assert.ok(dirents.some((d) => d.name === 'loop' && d.isSymbolicLink()));
  assert.ok(dirents.length < 100, `unbounded result: ${dirents.length}`);
}

// Async promises.readdir variant must reject-free terminate as well.
{
  const v = vfs.create();
  v.mkdirSync('/dir');
  v.symlinkSync('/dir', '/dir/loop');

  v.promises.readdir('/', { recursive: true }).then(common.mustCall((entries) => {
    assert.ok(entries.includes('dir/loop'));
    assert.ok(entries.length < 100, `unbounded result: ${entries.length}`);
  }));
}
