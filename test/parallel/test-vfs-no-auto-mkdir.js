'use strict';

// Verify create-mode VFS writes do NOT auto-create missing parent directories.
// Real fs fails with ENOENT when parent directories don't exist, and VFS
// must match this behavior.

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// writeFileSync to a non-existent parent must throw ENOENT.
{
  const myVfs = vfs.create();
  myVfs.mount('/mnt');

  assert.throws(
    () => fs.writeFileSync('/mnt/a/b/file.txt', 'x'),
    { code: 'ENOENT' },
  );

  myVfs.unmount();
}

// openSync with 'w' flag to non-existent parent throws ENOENT.
{
  const myVfs = vfs.create();
  myVfs.mount('/mnt');

  assert.throws(
    () => fs.openSync('/mnt/nodir/file.txt', 'w'),
    { code: 'ENOENT' },
  );

  myVfs.unmount();
}
