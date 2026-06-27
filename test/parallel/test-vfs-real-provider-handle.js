// Flags: --experimental-vfs --expose-internals
'use strict';

// RealFileHandle: sync and async file-handle operations, plus EBADF
// behaviour after close.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');
const { getVirtualFd } = require('internal/vfs/fd');

tmpdir.refresh();
const root = path.join(tmpdir.path, 'real-handle');
fs.mkdirSync(root, { recursive: true });
const myVfs = vfs.create(new vfs.RealFSProvider(root));

(async () => {
  // ===== Sync read/write/stat/truncate via openSync + getVirtualFd =====
  {
    fs.writeFileSync(path.join(root, 'sync-rw.txt'), 'hello world');
    const fd = myVfs.openSync('/sync-rw.txt', 'r+');
    const handle = getVirtualFd(fd).entry;

    const buf = Buffer.alloc(5);
    assert.strictEqual(handle.readSync(buf, 0, 5, 0), 5);
    assert.strictEqual(buf.toString(), 'hello');

    const wbuf = Buffer.from('zz');
    assert.strictEqual(handle.writeSync(wbuf, 0, 2, 0), 2);

    assert.strictEqual(handle.statSync().isFile(), true);
    assert.strictEqual(handle.readFileSync('utf8'), 'zzllo world');

    handle.writeFileSync('replaced');
    assert.strictEqual(handle.readFileSync('utf8'), 'replaced');

    myVfs.closeSync(fd);
  }

  // ===== Async read/write/stat/truncate via provider.open =====
  {
    await myVfs.promises.writeFile('/h2.txt', 'abcdef');
    const handle = await myVfs.provider.open('/h2.txt', 'r+');

    const buf = Buffer.alloc(3);
    assert.strictEqual(handle.readSync(buf, 0, 3, 0), 3);
    assert.strictEqual(buf.toString(), 'abc');

    const r = await handle.read(Buffer.alloc(3), 0, 3, 3);
    assert.strictEqual(r.bytesRead, 3);
    assert.strictEqual(r.buffer.toString(), 'def');

    handle.writeSync(Buffer.from('ZZ'), 0, 2, 0);
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

  // ===== readFile through an open real fd survives backing path rename =====
  {
    fs.writeFileSync(path.join(root, 'rename-read.txt'), 'still readable');
    const syncHandle = await myVfs.provider.open('/rename-read.txt', 'r');
    const asyncHandle = await myVfs.provider.open('/rename-read.txt', 'r');
    fs.renameSync(path.join(root, 'rename-read.txt'),
                  path.join(root, 'rename-read-renamed.txt'));
    try {
      assert.strictEqual(syncHandle.readFileSync('utf8'), 'still readable');
      assert.strictEqual(await asyncHandle.readFile('utf8'), 'still readable');
    } finally {
      await syncHandle.close();
      await asyncHandle.close();
      fs.unlinkSync(path.join(root, 'rename-read-renamed.txt'));
    }
  }

  // ===== readFile reads past the fallback chunk when fstat reports size 0 =====
  {
    const content = 'a'.repeat(8192) + 'trailing data';
    fs.writeFileSync(path.join(root, 'zero-stat.txt'), content);
    const syncHandle = await myVfs.provider.open('/zero-stat.txt', 'r');
    const asyncHandle = await myVfs.provider.open('/zero-stat.txt', 'r');
    const originalFstatSync = fs.fstatSync;
    const originalFstat = fs.fstat;

    fs.fstatSync = common.mustCall(function fstatSync(...args) {
      const stats = originalFstatSync.apply(this, args);
      stats.size = 0;
      return stats;
    });
    fs.fstat = common.mustCall(function fstat(fd, options, callback) {
      return originalFstat.call(this, fd, options, (err, stats) => {
        if (stats) stats.size = 0;
        callback(err, stats);
      });
    });

    try {
      assert.strictEqual(syncHandle.readFileSync('utf8'), content);
      assert.strictEqual(await asyncHandle.readFile('utf8'), content);
    } finally {
      fs.fstatSync = originalFstatSync;
      fs.fstat = originalFstat;
      await syncHandle.close();
      await asyncHandle.close();
    }
  }

  // ===== EBADF after close =====
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
    assert.throws(() => handle.readFileSync(), { code: 'EBADF' });
    await assert.rejects(handle.readFile(), { code: 'EBADF' });
    assert.throws(() => handle.writeFileSync('x'), { code: 'EBADF' });
    await assert.rejects(handle.writeFile('x'), { code: 'EBADF' });
    assert.throws(() => handle.statSync(), { code: 'EBADF' });
    await assert.rejects(handle.stat(), { code: 'EBADF' });
    assert.throws(() => handle.truncateSync(), { code: 'EBADF' });
    await assert.rejects(handle.truncate(), { code: 'EBADF' });
    // Subsequent close is a no-op
    handle.closeSync();
    await handle.close();
  }
})().then(common.mustCall());
