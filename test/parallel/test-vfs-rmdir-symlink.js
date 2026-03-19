// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// rmdirSync on a symlink to a directory should throw ENOTDIR
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir');
  myVfs.symlinkSync('/dir', '/link');
  myVfs.mount('/mnt');

  assert.throws(
    () => fs.rmdirSync('/mnt/link'),
    { code: 'ENOTDIR' },
  );

  // Both the symlink and directory should still exist
  assert.ok(fs.existsSync('/mnt/link'));
  assert.ok(fs.existsSync('/mnt/dir'));

  myVfs.unmount();
}

// rmdir (callback) on a symlink to a directory should return ENOTDIR
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir');
  myVfs.symlinkSync('/dir', '/link');
  myVfs.mount('/mnt2');

  fs.rmdir('/mnt2/link', (err) => {
    assert.ok(err);
    assert.strictEqual(err.code, 'ENOTDIR');
    assert.ok(fs.existsSync('/mnt2/link'));
    assert.ok(fs.existsSync('/mnt2/dir'));
    myVfs.unmount();
  });
}
