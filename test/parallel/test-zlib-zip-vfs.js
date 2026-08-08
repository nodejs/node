// Flags: --experimental-vfs
'use strict';

// Exercises the node:zlib ZipFile read/write surface over a node:vfs virtual
// file descriptor. An in-memory archive is placed in a mounted MemoryProvider
// VFS and opened with ZipFile through its mounted path, so every fd operation
// ZipFile performs - positional read/write, fstat, ftruncate, open/close, in
// both the asynchronous and the synchronous flavour - is served by the VFS
// rather than a real OS fd. This validates that ZipFile's fd abstraction is
// complete enough to run unchanged on a purely virtual descriptor.

require('../common');

const assert = require('node:assert');
const zlib = require('node:zlib');
const fs = require('node:fs');
const path = require('node:path');
const { test } = require('node:test');
const vfs = require('node:vfs');

let mountCounter = 0;

async function buildArchive(entries, comment) {
  const chunks = [];
  for await (const chunk of zlib.createZipArchive(entries, comment)) chunks.push(chunk);
  return Buffer.concat(chunks);
}

// Places `archive` in a freshly mounted in-memory VFS and returns the mounted
// path of the archive file plus a cleanup that unmounts the VFS.
function mountArchive(archive) {
  const mountPoint = path.resolve(`/vfs-zip-${process.pid}-${mountCounter++}`);
  const memfs = vfs.create();
  memfs.writeFileSync('/archive.zip', archive); // provider-relative, before mount
  memfs.mount(mountPoint);
  return { vpath: path.join(mountPoint, 'archive.zip'), cleanup: () => memfs.unmount() };
}

test('ZipFile.open() reads a VFS-backed archive through an async virtual fd', async () => {
  const archive = await buildArchive([
    await zlib.ZipEntry.create('a.txt', Buffer.from('hello vfs')),
    await zlib.ZipEntry.create('dir/b.bin', Buffer.from([1, 2, 3, 4]), { method: 'store' }),
    await zlib.ZipEntry.create('z.txt', Buffer.from('Z'.repeat(4096)), { method: 'zstd' }),
  ]);
  const { vpath, cleanup } = mountArchive(archive);
  try {
    const zf = await zlib.ZipFile.open(vpath);
    try {
      assert.strictEqual(zf.size, 3);
      assert.deepStrictEqual([...zf.keys()].sort(), ['a.txt', 'dir/b.bin', 'z.txt']);

      // A lazy, file-backed entry: it retains no content and reads straight
      // from the virtual fd on demand.
      const a = await zf.get('a.txt');
      assert.strictEqual(a.rawContent, null);
      assert.strictEqual((await a.content()).toString(), 'hello vfs');
      assert.strictEqual(await zf.get('a.txt'), a); // Cached handle identity

      // The contentIterator() streams (decompressing on the way) from the fd.
      const chunks = [];
      for await (const c of (await zf.get('z.txt')).contentIterator()) chunks.push(c);
      assert.strictEqual(Buffer.concat(chunks).toString(), 'Z'.repeat(4096));

      // stream() resolves to a Readable over the virtual fd.
      const rs = await zf.stream('dir/b.bin');
      const out = [];
      for await (const c of rs) out.push(c);
      assert.deepStrictEqual(Buffer.concat(out), Buffer.from([1, 2, 3, 4]));
    } finally {
      await zf.close();
    }
  } finally {
    cleanup();
  }
});

test('ZipFile.openSync() reads a VFS-backed archive through a synchronous virtual fd', async () => {
  const archive = await buildArchive([
    await zlib.ZipEntry.create('a.txt', Buffer.from('sync hello')),
    await zlib.ZipEntry.create('b.bin', Buffer.from([9, 8, 7]), { method: 'store' }),
  ]);
  const { vpath, cleanup } = mountArchive(archive);
  try {
    const zf = zlib.ZipFile.openSync(vpath);
    try {
      assert.strictEqual(zf.getSync('a.txt').contentSync().toString(), 'sync hello');
      assert.deepStrictEqual([...zf.getSync('b.bin').contentSync()], [9, 8, 7]);
      assert.deepStrictEqual([...zf.valuesSync()].map((e) => e.name).sort(), ['a.txt', 'b.bin']);
    } finally {
      zf.closeSync();
    }
  } finally {
    cleanup();
  }
});

test('ZipFile writable mutations run against an async virtual fd', async () => {
  const archive = await buildArchive([
    await zlib.ZipEntry.create('keep.txt', Buffer.from('keep')),
    await zlib.ZipEntry.create('drop.txt', Buffer.from('drop')),
  ]);
  const { vpath, cleanup } = mountArchive(archive);
  try {
    const sizeBefore = fs.statSync(vpath).size;
    const zw = await zlib.ZipFile.open(vpath, { writable: true });
    try {
      await zw.add('added.txt', Buffer.from('a new member over vfs'));
      assert.strictEqual(await zw.delete('drop.txt'), true);
    } finally {
      await zw.close();
    }
    // Positional writes + ftruncate actually altered the virtual file.
    assert.notStrictEqual(fs.statSync(vpath).size, sizeBefore);

    const zr = await zlib.ZipFile.open(vpath);
    try {
      assert.deepStrictEqual([...zr.keys()].sort(), ['added.txt', 'keep.txt']);
      assert.strictEqual((await (await zr.get('added.txt')).content()).toString(), 'a new member over vfs');
      assert.strictEqual(zr.has('drop.txt'), false);
    } finally {
      await zr.close();
    }
  } finally {
    cleanup();
  }
});

test('ZipFile sync mutations (addEntrySync) run against a synchronous virtual fd', async () => {
  const archive = await buildArchive([await zlib.ZipEntry.create('a.txt', Buffer.from('a'))]);
  const { vpath, cleanup } = mountArchive(archive);
  try {
    const zw = zlib.ZipFile.openSync(vpath, { writable: true });
    try {
      zw.addSync('b.txt', Buffer.from('b sync'));
      assert.deepStrictEqual([...zw.keys()].sort(), ['a.txt', 'b.txt']);
    } finally {
      zw.closeSync();
    }
    const zr = zlib.ZipFile.openSync(vpath);
    try {
      assert.strictEqual(zr.getSync('b.txt').contentSync().toString(), 'b sync');
    } finally {
      zr.closeSync();
    }
  } finally {
    cleanup();
  }
});

test('addEntry() promotes a streaming entry to file-backed on a virtual fd', async () => {
  const archive = await buildArchive([await zlib.ZipEntry.create('seed.txt', Buffer.from('seed'))]);
  const { vpath, cleanup } = mountArchive(archive);
  try {
    const payload = 'streamed over vfs'.repeat(32);
    const zw = await zlib.ZipFile.open(vpath, { writable: true });
    try {
      async function* source() {
        yield Buffer.from(payload.slice(0, 5));
        yield Buffer.from(payload.slice(5));
      }
      const streamEntry = zlib.ZipEntry.createStream('s.txt', source());
      const returned = await zw.addEntry(streamEntry);
      assert.strictEqual(returned, streamEntry);

      // Promoted in place against the virtual fd: now readable and re-streamable.
      assert.strictEqual((await streamEntry.content()).toString(), payload);
      const it = [];
      for await (const c of streamEntry.contentIterator()) it.push(c);
      assert.strictEqual(Buffer.concat(it).toString(), payload);
    } finally {
      await zw.close();
    }

    const zr = await zlib.ZipFile.open(vpath);
    try {
      assert.strictEqual((await (await zr.get('s.txt')).content()).toString(), payload);
    } finally {
      await zr.close();
    }
  } finally {
    cleanup();
  }
});
