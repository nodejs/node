// Flags: --experimental-vfs
'use strict';

// Error paths in VFS streams: missing files, EBADF on closed fds,
// destroyed streams, and write to a path under a missing parent.

const common = require('../common');
const vfs = require('node:vfs');

const myVfs = vfs.create();

// Read of a nonexistent file emits 'error'
{
  const stream = myVfs.createReadStream('/missing.txt');
  stream.on('error', common.expectsError({
    code: 'ENOENT',
  }));
}

// Read on a stream whose fd has been pre-closed → EBADF on first _read
{
  myVfs.writeFileSync('/x.txt', 'hi');
  const fd = myVfs.openSync('/x.txt');
  const rs = myVfs.createReadStream('/x.txt', { fd, autoClose: false });
  myVfs.closeSync(fd);
  rs.on('error', common.expectsError({
    code: 'EBADF',
  }));
  rs.resume();
}

// Read stream with autoClose:true after the fd is invalidated — covers the
// close-error swallow path inside the stream's #close.
{
  myVfs.writeFileSync('/cl.txt', 'data');
  const fd = myVfs.openSync('/cl.txt');
  const rs = myVfs.createReadStream('/cl.txt', { fd, autoClose: true });
  myVfs.closeSync(fd);
  rs.on('error', common.expectsError());
  rs.resume();
}

// WriteStream synchronously failing to open → destroys on next tick
{
  const ws = myVfs.createWriteStream('/missing-dir/foo.txt', { flags: 'wx' });
  ws.on('error', common.expectsError());
}

// WriteStream destroyed before write() — covers the destroyed-true branch
// in _write.
{
  const ws = myVfs.createWriteStream('/wd.txt');
  ws.on('error', () => {});
  ws.destroy(new Error('boom'));
}

// _write errors when writeSync throws (closed fd)
{
  myVfs.writeFileSync('/wfd.txt', '');
  const fd = myVfs.openSync('/wfd.txt', 'w');
  const ws = myVfs.createWriteStream('/wfd.txt', { fd, autoClose: false });
  myVfs.closeSync(fd);
  ws.on('error', common.expectsError());
  ws.write('x');
}
