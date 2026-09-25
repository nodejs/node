'use strict';
// fs.readFile()/fs.promises.readFile() read files of up to one chunk
// (512 KiB) with a single thread pool round trip and hand larger files over
// to the chunked reader. This pins the observable behaviour around that
// boundary: contents, encodings, empty and size-misreporting files, error
// shapes (which syscall failed), abort handling, flags, and that file
// descriptors and user buffers keep taking their existing paths.
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const { promisify } = require('util');
// Callback API through a promise, so both APIs can be awaited the same way.
const readFileCb = promisify(fs.readFile);
const async_hooks = require('async_hooks');

tmpdir.refresh();

const kChunk = 512 * 1024;
const sizes = [0, 1, 4096, kChunk - 1, kChunk, kChunk + 1, 3 * kChunk + 17];
const files = new Map();
for (const size of sizes) {
  const file = tmpdir.resolve(`f-${size}.bin`);
  const buf = Buffer.alloc(size);
  for (let i = 0; i < size; i++) buf[i] = (i * 31 + size) & 0xff;
  fs.writeFileSync(file, buf);
  files.set(size, { file, buf });
}
const textFile = tmpdir.resolve('text.txt');
const text = 'héllo wörld ✓ \u{1F600}\n'.repeat(1000);
fs.writeFileSync(textFile, text);

async function main() {
  // Contents across the one-shot / chunked boundary, both APIs.
  for (const [size, { file, buf }] of files) {
    assert.deepStrictEqual(await fs.promises.readFile(file), buf, `promises size=${size}`);
    assert.deepStrictEqual(await new Promise((res, rej) => fs.readFile(file, (e, d) => (e ? rej(e) : res(d)))), buf,
                           `callback size=${size}`);
    // Explicit flags (string and numeric).
    assert.deepStrictEqual(await fs.promises.readFile(file, { flag: 'r' }), buf);
    assert.deepStrictEqual(await readFileCb(file, { flag: fs.constants.O_RDONLY }), buf);
  }

  // Encodings.
  assert.strictEqual(await fs.promises.readFile(textFile, 'utf8'), text);
  assert.strictEqual(await fs.promises.readFile(textFile, { encoding: 'latin1' }),
                     Buffer.from(text).toString('latin1'));
  assert.strictEqual(await readFileCb(textFile, 'base64'), Buffer.from(text).toString('base64'));
  fs.readFile(textFile, 'utf8', common.mustSucceed((d) => assert.strictEqual(d, text)));

  // Errors keep their syscall/code/path shape.
  const missing = tmpdir.resolve('does-not-exist');
  for (const read of [() => fs.promises.readFile(missing),
                      () => new Promise((res, rej) => fs.readFile(missing, (e) => (e ? rej(e) : res())))]) {
    await assert.rejects(read, (err) => {
      assert.strictEqual(err.code, 'ENOENT');
      assert.strictEqual(err.syscall, 'open');
      assert.strictEqual(err.path, missing);
      assert.match(err.message, /ENOENT: no such file or directory, open/);
      return true;
    });
  }
  if (!common.isWindows && !common.isAIX && !common.isFreeBSD) {
    // Reading a directory: open succeeds, read fails (as before: syscall 'read').
    // (On AIX and FreeBSD a directory can be opened and read.)
    for (const read of [() => fs.promises.readFile(tmpdir.path),
                        () => new Promise((res, rej) => fs.readFile(tmpdir.path, (e) => (e ? rej(e) : res())))]) {
      await assert.rejects(read, { code: 'EISDIR', syscall: 'read' });
    }
  }
  // Abort: already-aborted signals reject before touching the file; both APIs.
  {
    const signal = AbortSignal.abort();
    await assert.rejects(fs.promises.readFile(textFile, { signal }), { name: 'AbortError' });
    await assert.rejects(readFileCb(textFile, { signal }), { name: 'AbortError' });
    // Aborting during a large (chunked) read still works.
    const ac = new AbortController();
    const big = files.get(3 * kChunk + 17).file;
    const p = fs.promises.readFile(big, { signal: ac.signal });
    ac.abort();
    await assert.rejects(p, { name: 'AbortError' });
  }

  // File descriptors and FileHandles keep working (they take the existing path).
  {
    const { file, buf } = files.get(4096);
    const fd = fs.openSync(file, 'r');
    assert.deepStrictEqual(await new Promise((res, rej) => fs.readFile(fd, (e, d) => (e ? rej(e) : res(d)))), buf);
    fs.closeSync(fd);
    const fh = await fs.promises.open(file, 'r');
    assert.deepStrictEqual(await fh.readFile(), buf);
    assert.deepStrictEqual(await fs.promises.readFile(fh), Buffer.alloc(0));  // Position is at EOF now.
    await fh.close();
  }

  // Large files handed back to the chunked reader must not leak the fd:
  // read one many times and make sure we can still open files afterwards.
  {
    const { file, buf } = files.get(kChunk + 1);
    for (let i = 0; i < 64; i++) {
      assert.strictEqual((await fs.promises.readFile(file)).length, buf.length);
    }
    await Promise.all(Array.from({ length: 64 }, () => readFileCb(file)));
  }

  // Files whose reported size is wrong (Linux procfs/sysfs) are read completely.
  if (common.isLinux) {
    for (const file of ['/proc/self/status', '/proc/self/maps', '/sys/kernel/mm/transparent_hugepage/enabled']) {
      let sync;
      try { sync = fs.readFileSync(file); } catch { continue; }
      const viaPromise = await fs.promises.readFile(file);
      const viaCallback = await new Promise((res, rej) => fs.readFile(file, (e, d) => (e ? rej(e) : res(d))));
      if (file === '/sys/kernel/mm/transparent_hugepage/enabled') {
        assert.deepStrictEqual(viaPromise, sync);
        assert.deepStrictEqual(viaCallback, sync);
      } else {
        assert.ok(viaPromise.length > 100 && viaCallback.length > 100, file);
        if (file === '/proc/self/maps') assert.ok(viaCallback.length > 4096);
      }
    }
  }

  // async_hooks see the read as an FSREQCALLBACK-typed resource with proper
  // init/before/after/destroy, and the callback runs in that context.
  {
    const seen = new Map();
    const hook = async_hooks.createHook({
      init(id, type) { if (type === 'FSREQCALLBACK') seen.set(id, ['init']); },
      before(id) { seen.get(id)?.push('before'); },
      after(id) { seen.get(id)?.push('after'); },
      destroy(id) { seen.get(id)?.push('destroy'); },
    }).enable();
    await new Promise((res) => fs.readFile(textFile, common.mustSucceed(() => {
      assert.ok([...seen.keys()].includes(async_hooks.executionAsyncId()));
      res();
    })));
    await new Promise((r) => setImmediate(r));
    hook.disable();
    const complete = [...seen.values()].some((events) => events.join() === 'init,before,after,destroy');
    assert.ok(complete, JSON.stringify([...seen.values()]));
  }

  // options.buffer (user-supplied buffer) keeps its own path and semantics.
  {
    const { file, buf } = files.get(4096);
    const target = Buffer.alloc(8192);
    const result = await fs.promises.readFile(file, { buffer: target });
    assert.strictEqual(result.buffer, target.buffer);
    assert.deepStrictEqual(result, buf);
  }
}

main().then(common.mustCall());
