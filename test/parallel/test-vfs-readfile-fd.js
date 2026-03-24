'use strict';

// Verify fs.readFileSync(fd) works with virtual file descriptors.
// Previously readFileSync only accepted path strings for VFS, not fds.

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'hello');
  myVfs.mount('/mnt');

  const fd = fs.openSync('/mnt/file.txt', 'r');
  assert.strictEqual(fs.readFileSync(fd, 'utf8'), 'hello');
  fs.closeSync(fd);

  myVfs.unmount();
}
