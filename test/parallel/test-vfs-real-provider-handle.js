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

    // Like `filehandle.writeFile()`, this writes from the handle's current
    // position rather than replacing the file, so a shorter write over an
    // "r+" handle leaves the tail of the old content in place.
    handle.writeFileSync('replaced');
    assert.strictEqual(handle.readFileSync('utf8'), 'replacedrld');

    myVfs.closeSync(fd);
  }

  // ===== writeFile goes through the file description, not the path =====
  {
    fs.writeFileSync(path.join(root, 'renamed-away.txt'), 'aaaaaa');
    const handle = await myVfs.provider.open('/renamed-away.txt', 'r+');
    fs.renameSync(path.join(root, 'renamed-away.txt'),
                  path.join(root, 'renamed-to.txt'));

    handle.writeFileSync('bb');
    await handle.writeFile('cc');
    await handle.close();

    assert.strictEqual(
      fs.readFileSync(path.join(root, 'renamed-to.txt'), 'utf8'), 'bbccaa');
    assert.strictEqual(fs.existsSync(path.join(root, 'renamed-away.txt')),
                       false);
  }

  // ===== writeFile takes the iterables filehandle.writeFile() takes =====
  {
    const handle = await myVfs.provider.open('/iterable.txt', 'w');
    await handle.writeFile(['one ', 'two ']);
    await handle.writeFile(async function* () {
      yield 'three ';
      yield Buffer.from('four');
    }());
    // A chunk that is not a view is converted the way writeFileHandle() does.
    await handle.writeFile([[32, 65], Uint8Array.of(66).buffer]);
    await handle.close();

    assert.strictEqual(
      fs.readFileSync(path.join(root, 'iterable.txt'), 'utf8'),
      'one two three four AB');
  }

  // ===== options are validated before the source is consumed =====
  {
    const handle = await myVfs.provider.open('/opts.txt', 'w');

    // The signal is read before the first next(), so a source that never
    // yields cannot leave the write pending.
    const neverYields = {
      [Symbol.asyncIterator]: () => ({ next: () => new Promise(() => {}) }),
    };
    await assert.rejects(
      handle.writeFile(neverYields, { signal: AbortSignal.abort() }),
      { name: 'AbortError' });

    // The rest of `options` is validated there too, so a bad value is
    // reported instead of waiting on a source that never produces.
    await assert.rejects(handle.writeFile(neverYields, { mode: 'invalid' }),
                         { code: 'ERR_INVALID_ARG_VALUE' });

    await handle.close();
  }

  // ===== flush costs one fsync per call, not one per chunk =====
  {
    const originalFsync = fs.fsync;
    let fsyncs = 0;
    fs.fsync = function fsync(...args) {
      fsyncs++;
      return originalFsync.apply(this, args);
    };

    try {
      const handle = await myVfs.provider.open('/flushed.txt', 'w');
      await handle.writeFile(['a', 'b', 'c'], { flush: true });
      await handle.close();
      assert.strictEqual(fsyncs, 1);
      assert.strictEqual(
        fs.readFileSync(path.join(root, 'flushed.txt'), 'utf8'), 'abc');

    } finally {
      fs.fsync = originalFsync;
    }
  }

  // ===== an abort landing during a write stops the source =====
  {
    const handle = await myVfs.provider.open('/abort-mid.txt', 'w');
    const ac = new AbortController();
    const originalWrite = fs.write;
    // Abort as the write settles, which is the window the post-write check
    // covers. Without it a source of one chunk resolves successfully.
    fs.write = function write(fd, buf, off, len, pos, callback) {
      return originalWrite.call(this, fd, buf, off, len, pos, (err, n) => {
        ac.abort();
        callback(err, n);
      });
    };

    let pulled = 0;
    try {
      await assert.rejects(handle.writeFile(async function* () {
        pulled++;
        yield 'first';
        pulled++;
        yield 'second';
      }(), { signal: ac.signal }), { name: 'AbortError' });
    } finally {
      fs.write = originalWrite;
      await handle.close();
    }
    assert.strictEqual(pulled, 1);  // The source was not asked for more
  }

  // ===== writeFile on a handle that was not opened for writing =====
  {
    fs.writeFileSync(path.join(root, 'ronly.txt'), 'untouched');
    const handle = await myVfs.provider.open('/ronly.txt', 'r');

    assert.throws(() => handle.writeFileSync('x'), { code: 'EBADF' });
    await assert.rejects(handle.writeFile('x'), { code: 'EBADF' });
    await handle.close();

    assert.strictEqual(
      fs.readFileSync(path.join(root, 'ronly.txt'), 'utf8'), 'untouched');
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

    // Each write starts where the previous one left the handle, so the
    // second call appends rather than replacing what the first one wrote.
    handle.writeFileSync('OVERWRITTEN');
    assert.strictEqual(handle.readFileSync('utf8'), 'OVERWRITTEN');
    await handle.writeFile('async-overwrite');
    assert.strictEqual(await handle.readFile('utf8'),
                       'OVERWRITTENasync-overwrite');

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
