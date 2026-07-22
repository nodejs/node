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

// Recursive readdir avoids following symlink cycles indefinitely.
{
  const v = vfs.create();
  v.mkdirSync('/dir');
  v.writeFileSync('/dir/nested.txt', 'nested');
  v.symlinkSync('/dir', '/dir/loop');

  assert.deepStrictEqual(v.readdirSync('/', { recursive: true }).sort(), [
    'dir',
    'dir/loop',
    'dir/nested.txt',
  ]);

  const dirents = v.readdirSync('/', { recursive: true, withFileTypes: true });
  assert.strictEqual(dirents.length, 3);
  assert.ok(dirents.some((dirent) =>
    dirent.name === 'loop' &&
    dirent.parentPath === '/dir' &&
    dirent.isSymbolicLink()));
}

// Recursive readdir avoids cycles through multiple symlinks.
{
  const v = vfs.create();
  v.mkdirSync('/a');
  v.mkdirSync('/b');
  v.symlinkSync('/b', '/a/link_to_b');
  v.symlinkSync('/a', '/b/link_to_a');

  assert.deepStrictEqual(v.readdirSync('/', { recursive: true }).sort(), [
    'a',
    'a/link_to_b',
    'a/link_to_b/link_to_a',
    'b',
    'b/link_to_a',
    'b/link_to_a/link_to_b',
  ]);
}

(async () => {
  const v = vfs.create();
  v.mkdirSync('/dir');
  v.writeFileSync('/dir/nested.txt', 'nested');
  v.symlinkSync('/dir', '/dir/loop');

  const entries = await v.promises.readdir('/', { recursive: true });
  assert.deepStrictEqual(entries.sort(), [
    'dir',
    'dir/loop',
    'dir/nested.txt',
  ]);
})().then(common.mustCall());

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

// Recursive readdir on a deeply nested tree must not exhaust the call stack.
// The iterative traversal introduced in this fix handles arbitrarily deep trees
// without recursion, so this should complete without a RangeError.
{
  const DEPTH = 1000;
  const v = vfs.create();

  // Build /deep/0/1/2/.../999/leaf.txt
  let path = '/deep';
  v.mkdirSync(path);
  for (let i = 0; i < DEPTH; i++) {
    path += `/${i}`;
    v.mkdirSync(path);
  }
  v.writeFileSync(`${path}/leaf.txt`, 'deep');

  const entries = v.readdirSync('/deep', { recursive: true });

  // Every intermediate directory and the leaf file must appear.
  assert.strictEqual(entries.length, DEPTH + 1);

  // Build the expected relative path to the leaf file.
  const expectedLeaf = Array.from({ length: DEPTH }, (_, i) => i).join('/') + '/leaf.txt';
  assert.ok(
    entries.includes(expectedLeaf),
    `Expected '${expectedLeaf}' in deep-tree entries`,
  );
}
