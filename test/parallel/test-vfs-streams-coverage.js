'use strict';

// Cover stream paths not exercised by other tests:
// - write/read on destroyed/closed streams (EBADF)
// - empty file read (push(null) early path)
// - WriteStream with explicit fd + start position
// - close() error swallowed

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();

// Empty file → ReadStream pushes null on the first read (remaining <= 0)
{
  myVfs.writeFileSync('/empty.txt', '');
  const rs = myVfs.createReadStream('/empty.txt');
  rs.on('data', () => assert.fail('no data expected'));
  rs.on('end', common.mustCall());
}

// Read on a stream whose fd has been pre-closed → EBADF on first _read
{
  myVfs.writeFileSync('/x.txt', 'hi');
  const fd = myVfs.openSync('/x.txt');
  const rs = myVfs.createReadStream('/x.txt', { fd, autoClose: false });
  // Close the fd before the stream's nextTick 'open' event runs.
  // The first _read will see the now-invalid fd in the lazy load path.
  myVfs.closeSync(fd);
  rs.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'EBADF');
  }));
  rs.resume(); // trigger _read
}

// WriteStream with explicit fd + start position
(async () => {
  myVfs.writeFileSync('/pad.txt', 'AAAAAAAAAA');
  const fd = myVfs.openSync('/pad.txt', 'r+');
  const ws = myVfs.createWriteStream('/pad.txt', { fd, start: 5, autoClose: false });
  await new Promise((resolve) => ws.on('ready', resolve));
  await new Promise((resolve, reject) =>
    ws.end('XX', (err) => err ? reject(err) : resolve()));
  myVfs.closeSync(fd);
  // Position 5 → "AAAAAXXAAA"
  assert.strictEqual(myVfs.readFileSync('/pad.txt', 'utf8'), 'AAAAAXXAAA');
})().then(common.mustCall());

// WriteStream synchronously failing to open → destroys on next tick
{
  // openSync on /missing-dir/file.txt without recursive parents fails ENOENT
  const ws = myVfs.createWriteStream('/missing-dir/foo.txt', { flags: 'wx' });
  ws.on('error', common.mustCall((err) => {
    assert.ok(err);
  }));
}

// _write errors when writeSync throws (closed fd)
{
  myVfs.writeFileSync('/wfd.txt', '');
  const fd = myVfs.openSync('/wfd.txt', 'w');
  const ws = myVfs.createWriteStream('/wfd.txt', { fd, autoClose: false });
  myVfs.closeSync(fd);
  ws.on('error', common.mustCall((err) => {
    assert.ok(err);
  }));
  ws.write('x');
}

// Read stream where the lazy read (vfd.entry.readFileSync) throws.
// Externally close the underlying virtual fd before _read runs but AFTER
// the constructor has stashed it, so vfd lookup succeeds but the entry
// read fails. We can simulate by destroying the virtual fd after the
// stream is created with autoClose:false.
{
  myVfs.writeFileSync('/lz.txt', 'data');
  const fd = myVfs.openSync('/lz.txt');
  const rs = myVfs.createReadStream('/lz.txt', { fd, autoClose: true });
  rs.on('error', common.mustCall(() => {}));
  // Trigger _read on next tick; before that, close the fd via the vfs
  // so the lazy lookup hits `if (!vfd)` (already covered) but #close in
  // _destroy will swallow its own duplicate-close error.
  myVfs.closeSync(fd);
  rs.resume();
}

// Read stream with autoClose:true and an error during _read — covers
// the close-error swallow path inside #close.
{
  myVfs.writeFileSync('/cl.txt', 'data');
  const fd = myVfs.openSync('/cl.txt');
  const rs = myVfs.createReadStream('/cl.txt', { fd, autoClose: true });
  myVfs.closeSync(fd);
  rs.on('error', common.mustCall(() => {}));
  rs.resume();
}

// WriteStream destroyed before write() — covers the destroyed-true branch
// in _write.
{
  const ws = myVfs.createWriteStream('/wd.txt');
  ws.on('error', () => {});
  ws.destroy(new Error('boom'));
}

// Read stream with explicit start beyond file end → remaining <= 0 → push null
{
  myVfs.writeFileSync('/sm.txt', 'abc');
  const rs = myVfs.createReadStream('/sm.txt', { start: 10 });
  rs.on('data', () => assert.fail('no data expected'));
  rs.on('end', common.mustCall());
}
