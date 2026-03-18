'use strict';

// Tests for VirtualDir handle behavior:
// - closeSync() throws ERR_DIR_CLOSED on double close
// - read(cb) and close(cb) callbacks fire correctly

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// Calling closeSync() twice on the same dir handle should throw
// ERR_DIR_CLOSED.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir');
  myVfs.mount('/mnt');

  const dir = fs.opendirSync('/mnt/dir');
  dir.closeSync();
  assert.throws(() => dir.closeSync(), { code: 'ERR_DIR_CLOSED' });

  myVfs.unmount();
}

// Dir.read(cb) and Dir.close(cb) callbacks fire on VFS directories.
// Previously, callback arguments were ignored and the callbacks never fired.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir');
  myVfs.writeFileSync('/dir/file.txt', 'x');
  myVfs.mount('/mnt');

  const dir = fs.opendirSync('/mnt/dir');

  dir.read(common.mustSucceed((ent) => {
    assert.strictEqual(ent.name, 'file.txt');

    dir.read(common.mustSucceed((ent2) => {
      assert.strictEqual(ent2, null); // No more entries

      dir.close(common.mustSucceed(() => {
        // Close succeeded
      }));
    }));
  }));

  myVfs.unmount();
}
