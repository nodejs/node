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
