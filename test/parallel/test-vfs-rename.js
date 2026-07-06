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
