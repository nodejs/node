'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const baseMountPoint = path.resolve('/tmp/vfs-realpath-native-' + process.pid);
let mountCounter = 0;

function createMountedVfs() {
  const mountPoint = baseMountPoint + '-' + (mountCounter++);
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir', { recursive: true });
  myVfs.writeFileSync('/dir/target.txt', 'content');
  myVfs.symlinkSync('/dir/target.txt', '/link.txt');
  myVfs.mount(mountPoint);
  return { myVfs, mountPoint };
}

// Test realpathSync.native resolves VFS symlinks
{
  const { myVfs, mountPoint } = createMountedVfs();
  const result = fs.realpathSync.native(path.join(mountPoint, 'link.txt'));
  assert.strictEqual(result, path.join(mountPoint, 'dir/target.txt'));
  myVfs.unmount();
}

// Test realpathSync.native with encoding: 'buffer'
{
  const { myVfs, mountPoint } = createMountedVfs();
  const result = fs.realpathSync.native(
    path.join(mountPoint, 'link.txt'),
    { encoding: 'buffer' },
  );
  assert.ok(Buffer.isBuffer(result));
  assert.strictEqual(
    result.toString(),
    path.join(mountPoint, 'dir/target.txt'),
  );
  myVfs.unmount();
}

// Test realpath.native resolves VFS symlinks (async)
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.realpath.native(
    path.join(mountPoint, 'link.txt'),
    common.mustSucceed((resolvedPath) => {
      assert.strictEqual(resolvedPath, path.join(mountPoint, 'dir/target.txt'));
      myVfs.unmount();
    }),
  );
}

// Test realpath.native with encoding: 'buffer' (async)
{
  const { myVfs, mountPoint } = createMountedVfs();
  fs.realpath.native(
    path.join(mountPoint, 'link.txt'),
    { encoding: 'buffer' },
    common.mustSucceed((resolvedPath) => {
      assert.ok(Buffer.isBuffer(resolvedPath));
      assert.strictEqual(
        resolvedPath.toString(),
        path.join(mountPoint, 'dir/target.txt'),
      );
      myVfs.unmount();
    }),
  );
}
