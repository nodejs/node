'use strict';

// zlib.zipFiles(): build an archive from files on disk, capturing each file's
// mode and modification time, streaming regular-file contents, and either
// following symlinks (default) or storing them as symlink entries.

const common = require('../common');

const assert = require('node:assert');
const zlib = require('node:zlib');
const fs = require('node:fs');
const fsp = require('node:fs/promises');
const tmpdir = require('../common/tmpdir');
const path = require('node:path');
const { test } = require('node:test');

tmpdir.refresh();

async function collect(readable) {
  const chunks = [];
  for await (const chunk of readable) chunks.push(chunk);
  return Buffer.concat(chunks);
}

// Disposal of a streaming source completes asynchronously - its descriptor is
// closed on the libuv threadpool - and an abandoned archive tears its queued
// sources down one after another, each waiting on the previous close. So the
// moment "every source is destroyed" can trail the destroy() call, arbitrarily
// far on a loaded machine. Wait for that real end state rather than assuming a
// fixed delay has been long enough.
async function waitForAllDestroyed(streams) {
  const deadline = Date.now() + common.platformTimeout(5000);
  while (streams.some((s) => !s.destroyed) && Date.now() < deadline) {
    await new Promise((resolve) => setTimeout(resolve, 5));
  }
}

// Write `count` throwaway files and return streaming entries backed by their
// (eagerly opened) read streams, so a test can observe whether each source is
// destroyed. The worst case for leaks: every descriptor is open before the
// archive starts.
async function streamingEntries(count) {
  const dir = await fsp.mkdtemp(path.join(tmpdir.path, 'zlib-zip-dispose-'));
  const streams = [];
  const entries = [];
  for (let i = 0; i < count; i++) {
    const p = path.join(dir, `f${i}.bin`);
    await fsp.writeFile(p, Buffer.alloc(1024 * 1024, i));
    const stream = fs.createReadStream(p);
    streams.push(stream);
    entries.push(zlib.ZipEntry.createStream(`f${i}.bin`, stream, { method: 'store' }));
  }
  return { dir, streams, entries };
}

async function makeTree() {
  const dir = await fsp.mkdtemp(path.join(tmpdir.path, 'zlib-zipfiles-'));
  await fsp.writeFile(path.join(dir, 'a.txt'), 'alpha');
  await fsp.chmod(path.join(dir, 'a.txt'), 0o640);
  await fsp.mkdir(path.join(dir, 'sub'));
  await fsp.writeFile(path.join(dir, 'sub', 'b.bin'), Buffer.from([1, 2, 3, 4]));
  return dir;
}

test('zipFiles archives files, directories, contents and Unix mode from disk', async () => {
  const dir = await makeTree();
  try {
    const files = [
      [path.join(dir, 'a.txt'), 'a.txt'],
      [path.join(dir, 'sub'), 'sub'],
      [path.join(dir, 'sub', 'b.bin'), 'nested/b.bin'],
    ];
    using zip = new zlib.ZipBuffer(await collect(zlib.zipFiles(files)));
    assert.deepStrictEqual([...zip.keys()].sort(), ['a.txt', 'nested/b.bin', 'sub/']);
    assert.strictEqual(zip.get('a.txt').contentSync().toString(), 'alpha');
    // Windows has no POSIX permission bits (chmod only toggles read-only, and
    // stat reports 0o666), so the exact captured mode is Unix-specific.
    if (!common.isWindows) {
      assert.strictEqual(zip.get('a.txt').mode, 0o640);
    }
    assert.strictEqual(zip.get('sub/').isDirectory, true);
    assert.deepStrictEqual([...zip.get('nested/b.bin').contentSync()], [1, 2, 3, 4]);
  } finally {
    await fsp.rm(dir, { recursive: true, force: true });
  }
});

test('zipFiles accepts any iterable of [path, name] (array, Map, Object.entries)', async () => {
  const dir = await makeTree();
  try {
    const p = path.join(dir, 'a.txt');
    const inputs = [
      [[p, 'a.txt']],                       // array of pairs
      new Map([[p, 'a.txt']]),              // Map
      Object.entries({ [p]: 'a.txt' }),     // Object.entries
    ];
    for (const files of inputs) {
      using zip = new zlib.ZipBuffer(await collect(zlib.zipFiles(files)));
      assert.strictEqual(zip.get('a.txt').contentSync().toString(), 'alpha');
    }
  } finally {
    await fsp.rm(dir, { recursive: true, force: true });
  }
});

test('zipFiles preserves a sub-second modification time via an extra field', async () => {
  const dir = await makeTree();
  try {
    const p = path.join(dir, 'a.txt');
    // 1-second-and-a-half past an epoch second, so the DOS field alone can't
    // represent it and the extended-timestamp extra field is what carries it.
    const when = new Date(1700000000500);
    await fsp.utimes(p, when, when);
    using zip = new zlib.ZipBuffer(await collect(zlib.zipFiles([[p, 'a.txt']])));
    assert.strictEqual(zip.get('a.txt').modified.getTime(), 1700000000000);
  } finally {
    await fsp.rm(dir, { recursive: true, force: true });
  }
});

test('zipFiles follows symlinks by default, or stores them when disabled', {
  skip: !common.canCreateSymLink() && 'insufficient privileges to create symlinks',
}, async () => {
  const dir = await makeTree();
  try {
    await fsp.symlink('a.txt', path.join(dir, 'link'));
    const files = [[path.join(dir, 'link'), 'link']];

    // Default: the link is resolved and archived as the target file.
    using followed = new zlib.ZipBuffer(await collect(zlib.zipFiles(files)));
    assert.strictEqual(followed.get('link').isSymlink, false);
    assert.strictEqual(followed.get('link').isFile, true);
    assert.strictEqual(followed.get('link').contentSync().toString(), 'alpha');

    // followSymlinks: false: the link itself becomes a symlink entry.
    using stored = new zlib.ZipBuffer(await collect(zlib.zipFiles(files, { followSymlinks: false })));
    const link = stored.get('link');
    assert.strictEqual(link.isSymlink, true);
    assert.strictEqual(link.isFile, false);
    assert.strictEqual(link.contentSync().toString(), 'a.txt');
  } finally {
    await fsp.rm(dir, { recursive: true, force: true });
  }
});

test('zipFiles rejects when a listed path does not exist', async () => {
  const dir = await makeTree();
  try {
    await assert.rejects(collect(zlib.zipFiles([[path.join(dir, 'missing'), 'x']])),
                         { code: 'ENOENT' });
  } finally {
    await fsp.rm(dir, { recursive: true, force: true });
  }
});

// -- entry ownership and disposal ----------------------------------------------

test('an abandoned createZipArchive() destroys the sources of entries it never reached', async () => {
  const { dir, streams, entries } = await streamingEntries(5);
  try {
    const archive = zlib.createZipArchive(entries);
    archive.on('error', () => {});
    let seen = 0;
    for await (const chunk of archive) {
      seen += chunk.length;
      if (seen > 256 * 1024) break; // Bail while still inside the first member
    }
    archive.destroy();
    await waitForAllDestroyed(streams);
    const open = streams.filter((s) => !s.destroyed);
    assert.strictEqual(open.length, 0, `${open.length} source streams left open`);
  } finally {
    await fsp.rm(dir, { recursive: true, force: true });
  }
});

test('a fully consumed archive leaves a valid result and ends every source', async () => {
  const { dir, streams, entries } = await streamingEntries(4);
  try {
    const archive = Buffer.concat(await (async () => {
      const chunks = [];
      for await (const chunk of zlib.createZipArchive(entries)) chunks.push(chunk);
      return chunks;
    })());
    using zip = new zlib.ZipBuffer(archive);
    assert.strictEqual(zip.size, 4);
    assert.strictEqual((await zip.get('f2.bin').content()).length, 1024 * 1024);
    assert.strictEqual(streams.filter((s) => !s.destroyed).length, 0);
  } finally {
    await fsp.rm(dir, { recursive: true, force: true });
  }
});

test('zipEntry disposal destroys a streaming source, sync and async', async () => {
  const { dir, streams, entries } = await streamingEntries(2);
  try {
    entries[0][Symbol.dispose]();
    await entries[1][Symbol.asyncDispose]();
    await waitForAllDestroyed(streams);
    assert.strictEqual(streams[0].destroyed, true);
    assert.strictEqual(streams[1].destroyed, true);
  } finally {
    await fsp.rm(dir, { recursive: true, force: true });
  }
});

test('disposing an in-memory or file-backed entry is a harmless no-op', async () => {
  const mem = await zlib.ZipEntry.create('m.txt', Buffer.from('keep me'));
  mem[Symbol.dispose]();
  await mem[Symbol.asyncDispose]();
  assert.deepStrictEqual(await mem.content(), Buffer.from('keep me'));

  // A file-backed entry borrows the ZipFile's descriptor; disposing it must
  // not close that descriptor.
  const dir = await fsp.mkdtemp(path.join(tmpdir.path, 'zlib-zip-dispose-fb-'));
  const archivePath = path.join(dir, 'a.zip');
  try {
    await fsp.writeFile(archivePath, await collect(zlib.createZipArchive(
      [await zlib.ZipEntry.create('a.txt', Buffer.from('hello'))])));
    using zf = await zlib.ZipFile.open(archivePath);
    const entry = await zf.get('a.txt');
    entry[Symbol.dispose]();
    assert.strictEqual((await entry.content()).toString(), 'hello');
    assert.strictEqual((await (await zf.get('a.txt')).content()).toString(), 'hello');
  } finally {
    await fsp.rm(dir, { recursive: true, force: true });
  }
});

test('createZipArchiveSync() throws on a streaming entry and disposes the rest', async () => {
  const { dir, streams, entries } = await streamingEntries(3);
  try {
    assert.throws(() => Array.from(zlib.createZipArchiveSync(entries)),
                  { code: 'ERR_INVALID_STATE' });
    await waitForAllDestroyed(streams);
    assert.strictEqual(streams.filter((s) => !s.destroyed).length, 0);
  } finally {
    await fsp.rm(dir, { recursive: true, force: true });
  }
});
