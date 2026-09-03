// Flags: --experimental-vfs
'use strict';

// VFS dispatch on the async callback fs methods must surface provider errors
// through the callback, not as a synchronous throw or unhandled rejection.

const common = require('../common');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

function mounted() {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src', { recursive: true });
  myVfs.writeFileSync('/src/hello.txt', 'hello');
  const mountPoint = myVfs.mount();
  return { myVfs, mountPoint };
}

// fs.access on missing file inside a mount
{
  const { mountPoint } = mounted();
  fs.access(path.join(mountPoint, 'missing'),
            common.expectsError({ code: 'ENOENT' }));
}

// fs.lstat on missing file inside a mount.
// lstat passes (err) only on the error path, so expectsError works here.
{
  const { mountPoint } = mounted();
  fs.lstat(path.join(mountPoint, 'missing'),
           common.expectsError({ code: 'ENOENT' }));
}

// fs.open on a path whose parent directory does not exist
{
  const { mountPoint } = mounted();
  fs.open(path.join(mountPoint, 'missing-parent/x.txt'), 'wx',
          common.expectsError({ code: 'ENOENT' }));
}

// fs.read on a VFS fd that has been closed -> EBADF through callback.
// fs.read invokes the callback with (err, bytesRead, buffer), so the
// single-argument expectsError contract does not match - use mustCall here.
{
  const { mountPoint } = mounted();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  fs.closeSync(fd);
  fs.read(fd, Buffer.alloc(5), 0, 5, 0, common.mustCall((err) => {
    common.expectsError({ code: 'EBADF' })(err);
  }));
}

// fs.write on a VFS fd that has been closed -> EBADF through callback.
// fs.write invokes the callback with (err, bytesWritten, buffer); same
// rationale as fs.read above.
{
  const { mountPoint } = mounted();
  const fd = fs.openSync(path.join(mountPoint, 'src/w.txt'), 'w');
  fs.closeSync(fd);
  fs.write(fd, Buffer.from('x'), 0, 1, 0, common.mustCall((err) => {
    common.expectsError({ code: 'EBADF' })(err);
  }));
}

// fs.fstat on a VFS fd that has been closed -> EBADF through callback
{
  const { mountPoint } = mounted();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  fs.closeSync(fd);
  fs.fstat(fd, common.expectsError({ code: 'EBADF' }));
}
