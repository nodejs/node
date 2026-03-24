'use strict';

// Tests for VFS writeFile/appendFile flag support:
// - writeFileSync with { flag: 'wx' } throws EEXIST for existing files
// - appendFileSync with { flag: 'ax' } throws EEXIST for existing files

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// writeFileSync with { flag: 'wx' } must throw EEXIST for existing files.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'orig');
  myVfs.mount('/mnt');

  assert.throws(
    () => fs.writeFileSync('/mnt/file.txt', 'new', { flag: 'wx' }),
    { code: 'EEXIST' },
  );

  // Original content should be unchanged
  assert.strictEqual(fs.readFileSync('/mnt/file.txt', 'utf8'), 'orig');

  myVfs.unmount();
}

// appendFileSync with { flag: 'ax' } must throw EEXIST for existing files.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'orig');
  myVfs.mount('/mnt');

  assert.throws(
    () => fs.appendFileSync('/mnt/file.txt', 'extra', { flag: 'ax' }),
    { code: 'EEXIST' },
  );

  myVfs.unmount();
}
