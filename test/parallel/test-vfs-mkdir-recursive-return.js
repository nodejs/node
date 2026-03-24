'use strict';

// Verify mkdirSync({ recursive: true }) returns the first directory created.
// When some parent directories already exist, the return value should be the
// first newly-created directory path.

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// Unmounted variant: test through VFS API directly.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/a');

  const result = myVfs.mkdirSync('/a/b/c', { recursive: true });
  assert.strictEqual(result, '/a/b');
}

// Mounted variant: test through fs.mkdirSync with mounted VFS.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/a');
  myVfs.mount('/mnt');

  const result = fs.mkdirSync('/mnt/a/b/c', { recursive: true });
  // The result is the mounted path of the first directory created
  assert.strictEqual(result, '/mnt/a/b');

  myVfs.unmount();
}
