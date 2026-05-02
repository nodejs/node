'use strict';

// Cover RealFSProvider edge cases: path-escape rejection, RealFileHandle
// methods, error paths.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

tmpdir.refresh();
const root = path.join(tmpdir.path, 'real-coverage');
fs.mkdirSync(root, { recursive: true });

const myVfs = vfs.create(new vfs.RealFSProvider(root));

// RealFileHandle methods after close throw EBADF
(async () => {
  await myVfs.promises.writeFile('/h.txt', 'hello');
  const handle = await myVfs.provider.open('/h.txt', 'r');
  await handle.close();
  assert.strictEqual(handle.closed, true);
  assert.throws(() => handle.readSync(Buffer.alloc(1), 0, 1, 0),
                { code: 'EBADF' });
  await assert.rejects(handle.read(Buffer.alloc(1), 0, 1, 0),
                       { code: 'EBADF' });
  assert.throws(() => handle.writeSync(Buffer.from('x'), 0, 1, 0),
                { code: 'EBADF' });
  await assert.rejects(handle.write(Buffer.from('x'), 0, 1, 0),
                       { code: 'EBADF' });
  assert.throws(() => handle.readFileSync(),
                { code: 'EBADF' });
  await assert.rejects(handle.readFile(),
                       { code: 'EBADF' });
  assert.throws(() => handle.writeFileSync('x'),
                { code: 'EBADF' });
  await assert.rejects(handle.writeFile('x'),
                       { code: 'EBADF' });
  assert.throws(() => handle.statSync(),
                { code: 'EBADF' });
  await assert.rejects(handle.stat(),
                       { code: 'EBADF' });
  assert.throws(() => handle.truncateSync(),
                { code: 'EBADF' });
  await assert.rejects(handle.truncate(),
                       { code: 'EBADF' });
  // Subsequent close() / closeSync() are no-ops
  handle.closeSync();
  await handle.close();
})().then(common.mustCall());

// RealFileHandle read/write/stat/truncate happy path
(async () => {
  await myVfs.promises.writeFile('/h2.txt', 'abcdef');
  const handle = await myVfs.provider.open('/h2.txt', 'r+');
  assert.strictEqual(typeof handle.fd, 'number');

  const buf = Buffer.alloc(3);
  const r = handle.readSync(buf, 0, 3, 0);
  assert.strictEqual(r, 3);
  assert.strictEqual(buf.toString(), 'abc');

  const r2 = await handle.read(Buffer.alloc(3), 0, 3, 3);
  assert.strictEqual(r2.bytesRead, 3);
  assert.strictEqual(r2.buffer.toString(), 'def');

  const wbuf = Buffer.from('zz');
  handle.writeSync(wbuf, 0, 2, 0);
  const w = await handle.write(Buffer.from('YY'), 0, 2, 4);
  assert.strictEqual(w.bytesWritten, 2);

  // statSync / stat
  const s1 = handle.statSync();
  const s2 = await handle.stat();
  assert.strictEqual(s1.size, s2.size);

  // readFileSync / readFile (path-based, not fd-based)
  assert.ok(handle.readFileSync().length > 0);
  assert.ok((await handle.readFile()).length > 0);

  // writeFileSync / writeFile overwrite the entire file (path-based)
  handle.writeFileSync('OVERWRITTEN');
  assert.strictEqual(handle.readFileSync('utf8'), 'OVERWRITTEN');
  await handle.writeFile('async-overwrite');
  assert.strictEqual(await handle.readFile('utf8'), 'async-overwrite');

  // truncateSync / truncate
  handle.truncateSync(3);
  await handle.truncate(2);

  await handle.close();
})().then(common.mustCall());

// Path-escape rejection: VFS paths cannot escape rootPath via .. segments
(async () => {
  // ../ patterns get resolved into the root by path.resolve, so they never
  // actually escape — but we still verify the error surface.
  await assert.rejects(myVfs.promises.stat('/../../../etc/passwd'),
                       { code: 'ENOENT' });

  // Symbolic link that points outside the root → readlinkSync returns the
  // real (untranslated) target path; realpath rejects with EACCES because
  // the resolved path escaped root.
  fs.writeFileSync(path.join(tmpdir.path, 'outside.txt'), 'forbidden');
  fs.symlinkSync(path.join(tmpdir.path, 'outside.txt'),
                 path.join(root, 'esc-link'));

  const target = myVfs.readlinkSync('/esc-link');
  // Target is absolute and outside root → returned verbatim
  assert.strictEqual(target, path.join(tmpdir.path, 'outside.txt'));
  const target2 = await myVfs.promises.readlink('/esc-link');
  assert.strictEqual(target2, path.join(tmpdir.path, 'outside.txt'));

  // realpath through the escape-link rejects with ENOENT (the security
  // check at #resolvePath catches the escape after fs.realpathSync resolves
  // through it).
  assert.throws(() => myVfs.realpathSync('/esc-link'),
                { code: 'EACCES' });
  await assert.rejects(myVfs.promises.realpath('/esc-link'),
                       { code: 'EACCES' });
})().then(common.mustCall());

// Symlink with relative target (within root) — readlink returns target as-is
(async () => {
  fs.writeFileSync(path.join(root, 'rel-target.txt'), 'ok');
  fs.symlinkSync('rel-target.txt', path.join(root, 'rel-link'));
  assert.strictEqual(myVfs.readlinkSync('/rel-link'), 'rel-target.txt');
  assert.strictEqual(await myVfs.promises.readlink('/rel-link'),
                     'rel-target.txt');
})().then(common.mustCall());

// access (sync + async) on existing and missing files
(async () => {
  await myVfs.promises.writeFile('/acc.txt', 'x');
  myVfs.accessSync('/acc.txt');
  await myVfs.promises.access('/acc.txt');
  await assert.rejects(myVfs.promises.access('/missing.txt'),
                       { code: 'ENOENT' });
})().then(common.mustCall());
