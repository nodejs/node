'use strict';

// Cover RealFSProvider edge cases: path-escape rejection, RealFileHandle
// methods, error paths. Run sequentially to avoid fd-recycling races
// between independent (async () => {})() blocks.

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

(async () => {
  // RealFileHandle methods after close throw EBADF
  {
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
    handle.closeSync();
    await handle.close();
  }

  // RealFileHandle read/write/stat/truncate happy path
  {
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

    const s1 = handle.statSync();
    const s2 = await handle.stat();
    assert.strictEqual(s1.size, s2.size);

    assert.ok(handle.readFileSync().length > 0);
    assert.ok((await handle.readFile()).length > 0);

    handle.writeFileSync('OVERWRITTEN');
    assert.strictEqual(handle.readFileSync('utf8'), 'OVERWRITTEN');
    await handle.writeFile('async-overwrite');
    assert.strictEqual(await handle.readFile('utf8'), 'async-overwrite');

    handle.truncateSync(3);
    await handle.truncate(2);

    await handle.close();
  }

  // Path-escape rejection
  {
    await assert.rejects(myVfs.promises.stat('/../../../etc/passwd'),
                         { code: 'ENOENT' });

    fs.writeFileSync(path.join(tmpdir.path, 'outside.txt'), 'forbidden');
    fs.symlinkSync(path.join(tmpdir.path, 'outside.txt'),
                   path.join(root, 'esc-link'));

    const target = myVfs.readlinkSync('/esc-link');
    assert.strictEqual(target, path.join(tmpdir.path, 'outside.txt'));
    const target2 = await myVfs.promises.readlink('/esc-link');
    assert.strictEqual(target2, path.join(tmpdir.path, 'outside.txt'));

    assert.throws(() => myVfs.realpathSync('/esc-link'),
                  { code: 'EACCES' });
    await assert.rejects(myVfs.promises.realpath('/esc-link'),
                         { code: 'EACCES' });
  }

  // Relative-target symlink within root
  {
    fs.writeFileSync(path.join(root, 'rel-target.txt'), 'ok');
    fs.symlinkSync('rel-target.txt', path.join(root, 'rel-link'));
    assert.strictEqual(myVfs.readlinkSync('/rel-link'), 'rel-target.txt');
    assert.strictEqual(await myVfs.promises.readlink('/rel-link'),
                       'rel-target.txt');
  }

  // access existing/missing
  {
    await myVfs.promises.writeFile('/acc.txt', 'x');
    myVfs.accessSync('/acc.txt');
    await myVfs.promises.access('/acc.txt');
    await assert.rejects(myVfs.promises.access('/missing.txt'),
                         { code: 'ENOENT' });
  }

  // open async error
  {
    await assert.rejects(myVfs.provider.open('/missing.txt', 'r'),
                         { code: 'ENOENT' });
  }

  // RealFileHandle async fd-ops error paths via externally closed fd.
  // Run last so the freed fd doesn't get recycled into a sibling test.
  {
    await myVfs.promises.writeFile('/eb.txt', 'x');
    const handle = await myVfs.provider.open('/eb.txt', 'r+');
    fs.closeSync(handle.fd);
    await assert.rejects(handle.read(Buffer.alloc(1), 0, 1, 0),
                         { code: 'EBADF' });
    await assert.rejects(handle.write(Buffer.from('y'), 0, 1, 0),
                         { code: 'EBADF' });
    await assert.rejects(handle.stat(), { code: 'EBADF' });
    await assert.rejects(handle.truncate(0), { code: 'EBADF' });
    await assert.rejects(handle.close(), { code: 'EBADF' });
  }

  // Symlink with relative target outside root → EACCES
  {
    assert.throws(() =>
      myVfs.symlinkSync('../../escape', '/bad-link'),
    { code: 'EACCES' });
    await assert.rejects(
      myVfs.promises.symlink('../../escape', '/bad-link2'),
      { code: 'EACCES' },
    );
  }

  // realpath via second escape-link
  {
    fs.writeFileSync(path.join(tmpdir.path, 'outside2.txt'), 'forbid');
    fs.symlinkSync(path.join(tmpdir.path, 'outside2.txt'),
                   path.join(root, 'esc-link2'));
    assert.throws(() => myVfs.realpathSync('/esc-link2'),
                  { code: 'EACCES' });
    await assert.rejects(myVfs.promises.realpath('/esc-link2'),
                         { code: 'EACCES' });
  }

  // Symlink whose absolute target equals root → readlink returns '/'
  {
    fs.symlinkSync(root, path.join(root, 'root-link2'));
    assert.strictEqual(myVfs.readlinkSync('/root-link2'), '/');
  }

  // VFS path with leading-..-and-no-slash escapes via path.resolve
  // (covers the post-resolve security check that rejects with ENOENT).
  // Note: '/../etc' normalizes back to '/etc' under root via slice(1) +
  // path.resolve, so it stays inside root. To trigger the escape branch
  // we use a path that does NOT start with '/' so slice(1) leaves the
  // '..' intact.
  {
    const escapeProvider = new vfs.RealFSProvider(root);
    assert.throws(() => escapeProvider.statSync('../etc/passwd'),
                  { code: 'ENOENT' });
  }

  // RealFSProvider with a rootPath that ends in path.sep — exercises the
  // `endsWith(path.sep) ? rootPath : rootPath + sep` branch.
  {
    const trailingRoot = root + path.sep;
    fs.writeFileSync(path.join(root, 'tr.txt'), 'tr');
    const tProvider = new vfs.RealFSProvider(trailingRoot);
    assert.strictEqual(tProvider.readFileSync('/tr.txt', 'utf8'), 'tr');
  }
})().then(common.mustCall());
