// Flags: --experimental-vfs
'use strict';

// Rename behaviour: overwrite, type mismatches, same-parent rename.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// Renaming a file onto a directory throws EISDIR
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'x');
  myVfs.mkdirSync('/dir');
  assert.throws(() => myVfs.renameSync('/file.txt', '/dir'),
                { code: 'EISDIR' });
}

// Renaming a directory onto a file throws ENOTDIR
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'x');
  myVfs.mkdirSync('/dir');
  assert.throws(() => myVfs.renameSync('/dir', '/file.txt'),
                { code: 'ENOTDIR' });
}

// Renaming a file onto another file overwrites
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/a.txt', 'a');
  myVfs.writeFileSync('/b.txt', 'b');
  myVfs.renameSync('/a.txt', '/b.txt');
  assert.strictEqual(myVfs.readFileSync('/b.txt', 'utf8'), 'a');
  assert.strictEqual(myVfs.existsSync('/a.txt'), false);
}

// Renaming within the same parent directory
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/d');
  myVfs.writeFileSync('/d/a.txt', 'x');
  myVfs.renameSync('/d/a.txt', '/d/b.txt');
  assert.strictEqual(myVfs.existsSync('/d/a.txt'), false);
  assert.strictEqual(myVfs.existsSync('/d/b.txt'), true);
}

// Renaming a directory into its own descendant throws EINVAL
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/a/b', { recursive: true });
  myVfs.writeFileSync('/a/file.txt', 'data');

  assert.throws(() => myVfs.renameSync('/a', '/a/b/c'), { code: 'EINVAL' });
  assert.deepStrictEqual(myVfs.readdirSync('/'), ['a']);
  assert.strictEqual(myVfs.existsSync('/a'), true);
  assert.strictEqual(myVfs.existsSync('/a/b'), true);
  assert.strictEqual(myVfs.existsSync('/a/b/c'), false);
  assert.strictEqual(myVfs.readFileSync('/a/file.txt', 'utf8'), 'data');
}

// Renaming a directory onto a non-empty directory throws ENOTEMPTY
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src');
  myVfs.mkdirSync('/dst');
  myVfs.writeFileSync('/dst/keep.txt', 'keep');

  assert.throws(() => myVfs.renameSync('/src', '/dst'), { code: 'ENOTEMPTY' });
  assert.strictEqual(myVfs.readFileSync('/dst/keep.txt', 'utf8'), 'keep');
  assert.strictEqual(myVfs.existsSync('/src'), true);
}

// Renaming a directory onto an empty directory succeeds
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src');
  myVfs.writeFileSync('/src/a.txt', 'a');
  myVfs.mkdirSync('/dst');

  myVfs.renameSync('/src', '/dst');
  assert.strictEqual(myVfs.existsSync('/src'), false);
  assert.strictEqual(myVfs.readFileSync('/dst/a.txt', 'utf8'), 'a');
}

// Overwriting a file drops one of its links
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/a.txt', 'a');
  myVfs.writeFileSync('/b.txt', 'b');
  myVfs.linkSync('/b.txt', '/b-link.txt');
  assert.strictEqual(myVfs.statSync('/b-link.txt').nlink, 2);

  myVfs.renameSync('/a.txt', '/b.txt');
  assert.strictEqual(myVfs.statSync('/b-link.txt').nlink, 1);
  assert.strictEqual(myVfs.readFileSync('/b-link.txt', 'utf8'), 'b');
}

// Renaming a path onto itself is a no-op
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/a.txt', 'a');
  myVfs.mkdirSync('/d');
  myVfs.writeFileSync('/d/keep.txt', 'keep');

  myVfs.renameSync('/a.txt', '/a.txt');
  assert.strictEqual(myVfs.readFileSync('/a.txt', 'utf8'), 'a');
  assert.strictEqual(myVfs.statSync('/a.txt').nlink, 1);

  myVfs.renameSync('/d', '/d');
  assert.strictEqual(myVfs.readFileSync('/d/keep.txt', 'utf8'), 'keep');
}

// Renaming a hard link onto another link to the same file is a no-op
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/a.txt', 'a');
  myVfs.linkSync('/a.txt', '/b.txt');

  myVfs.renameSync('/a.txt', '/b.txt');
  assert.strictEqual(myVfs.existsSync('/a.txt'), true);
  assert.strictEqual(myVfs.existsSync('/b.txt'), true);
  assert.strictEqual(myVfs.statSync('/a.txt').nlink, 2);
}
