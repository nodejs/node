'use strict';

// Tests for VFS access/accessSync behavior:
// - Invalid mode argument throws ERR_INVALID_ARG_TYPE
// - X_OK on non-executable entries throws EACCES
// - R_OK on default-mode files succeeds

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// Passing an invalid mode to accessSync should throw ERR_INVALID_ARG_TYPE
// even when the path belongs to a mounted VFS.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'x');
  myVfs.mount('/mnt');

  assert.throws(
    () => fs.accessSync('/mnt/file.txt', 'x'),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );

  myVfs.unmount();
}

// accessSync with X_OK on a non-executable directory must throw EACCES.
{
  const { X_OK } = fs.constants;
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir', { mode: 0o644 });
  myVfs.mount('/mnt');

  assert.throws(
    () => fs.accessSync('/mnt/dir', X_OK),
    { code: 'EACCES' },
  );

  myVfs.unmount();
}

// accessSync with R_OK succeeds on default-mode files.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'x');
  myVfs.mount('/mnt');

  fs.accessSync('/mnt/file.txt', fs.constants.R_OK);

  myVfs.unmount();
}
