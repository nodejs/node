// Flags: --experimental-vfs
'use strict';

// rmdirSync on a symlink to a directory should throw ENOTDIR

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir');
  myVfs.symlinkSync('/dir', '/link');

  assert.throws(() => myVfs.rmdirSync('/link'),
                { code: 'ENOTDIR' });

  // Both the symlink and directory should still exist
  assert.ok(myVfs.existsSync('/link'));
  assert.ok(myVfs.existsSync('/dir'));
}

// promises.rmdir equivalent
(async () => {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir');
  myVfs.symlinkSync('/dir', '/link');

  await assert.rejects(myVfs.promises.rmdir('/link'),
                       { code: 'ENOTDIR' });
})().then(common.mustCall());
