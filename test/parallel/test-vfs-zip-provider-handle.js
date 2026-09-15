// Flags: --experimental-vfs
'use strict';

// Additional ZipProvider coverage beyond test-vfs-zip-provider.js:
// direct ZipFileHandle read/write/stat/truncate (both explicit and
// current position, append-mode positioning, buffer growth), readFile/
// writeFile encoding and data-type variants, compression-method-preserving
// rename() (methodOption()), ZipFile-backed synchronous delete, an explicit
// empty-directory open() EISDIR, the readdir child-directory dedup path, and
// a few error branches (EROFS on open(), ENOTDIR on rmdir(), ENOENT on
// rename()) not exercised by the base test.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const fsPromises = require('fs/promises');
const zlib = require('zlib');
const vfs = require('node:vfs');

tmpdir.refresh();

async function buildArchive(entries, comment) {
  const chunks = [];
  for await (const chunk of zlib.createZipArchive(entries, comment)) chunks.push(chunk);
  return Buffer.concat(chunks);
}

function buildArchiveSync(entries, comment) {
  return Buffer.concat([...zlib.createZipArchiveSync(entries, comment)]);
}

// --- direct ZipFileHandle read/write/stat/truncate (async) -------------
(async () => {
  const archive = await buildArchive([
    await zlib.ZipEntry.create('a.txt', Buffer.from('hello world')),
  ]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);

  const handle = await provider.open('/a.txt', 'r+');

  // Explicit position leaves the handle's own position untouched.
  const buf = Buffer.alloc(5);
  const { bytesRead } = await handle.read(buf, 0, 5, 0);
  assert.strictEqual(bytesRead, 5);
  assert.strictEqual(buf.toString(), 'hello');
  assert.strictEqual(handle.position, 0);

  // Current position (undefined/null/-1) advances the handle's position.
  const buf2 = Buffer.alloc(5);
  await handle.read(buf2, 0, 5, null);
  assert.strictEqual(buf2.toString(), 'hello');
  assert.strictEqual(handle.position, 5);
  const buf3 = Buffer.alloc(1);
  await handle.read(buf3, 0, 1, -1);
  assert.strictEqual(buf3.toString(), ' ');
  assert.strictEqual(handle.position, 6);

  // Reading past EOF yields 0 bytes without advancing further than available.
  const tail = Buffer.alloc(100);
  const { bytesRead: tailRead } = await handle.read(tail, 0, 100, 6);
  assert.strictEqual(tailRead, 5); // 'world'.length

  // write() at an explicit position (overwrite), then at the current
  // position (append past the buffer's current size, forcing growth).
  await handle.write(Buffer.from('HELLO'), 0, 5, 0);
  const stat1 = await handle.stat();
  assert.strictEqual(stat1.size, 11);

  handle.position = 11;
  await handle.write(Buffer.from('!!!'), 0, 3, null);
  const stat2 = await handle.stat();
  assert.strictEqual(stat2.size, 14);

  await handle.truncate(5);
  const stat3 = await handle.stat();
  assert.strictEqual(stat3.size, 5);

  await handle.truncate(8);
  const stat4 = await handle.stat();
  assert.strictEqual(stat4.size, 8);

  await handle.close();

  const archiveVfs = vfs.create(provider);
  const final = await archiveVfs.promises.readFile('/a.txt');
  assert.strictEqual(final.length, 8);
  assert.strictEqual(final.subarray(0, 5).toString(), 'HELLO');
})().then(common.mustCall());

// --- direct ZipFileHandle read/write/stat/truncate (sync) --------------
(() => {
  const archive = buildArchiveSync([zlib.ZipEntry.createSync('b.txt', Buffer.from('0123456789'))]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);
  const handle = provider.openSync('/b.txt', 'r+');

  const buf = Buffer.alloc(4);
  const { bytesRead } = handle.readSync(buf, 0, 4, 2);
  assert.strictEqual(bytesRead, 4);
  assert.strictEqual(buf.toString(), '2345');

  handle.writeSync(Buffer.from('AB'), 0, 2, 0);
  assert.strictEqual(handle.statSync().size, 10);

  // A write growing well beyond current capacity exercises #ensureCapacity's
  // doubling path.
  handle.writeSync(Buffer.alloc(1000, 0x58 /* 'X' */), 0, 1000, 10);
  assert.strictEqual(handle.statSync().size, 1010);

  handle.truncateSync(3);
  assert.strictEqual(handle.statSync().size, 3);
  assert.strictEqual(handle.readFileSync('utf8'), 'AB2');

  handle.closeSync();
})();

// --- append-mode positioning: writes always land at the current end -------
(async () => {
  const archive = await buildArchive([await zlib.ZipEntry.create('c.txt', Buffer.from('xy'))]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);

  const handle = await provider.open('/c.txt', 'a');
  assert.strictEqual(handle.position, 2); // Positioned at EOF on open

  // Even with an explicit (wrong) position, append mode writes at the end.
  await handle.write(Buffer.from('z'), 0, 1, 0);
  assert.strictEqual((await handle.stat()).size, 3);
  await handle.close();

  const archiveVfs = vfs.create(provider);
  assert.strictEqual(await archiveVfs.promises.readFile('/c.txt', 'utf8'), 'xyz');
})().then(common.mustCall());

// --- readFile/readFileSync encoding variants + writeFile data types --------
(async () => {
  const archive = await buildArchive([await zlib.ZipEntry.create('d.txt', Buffer.from('café'))]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);

  const handle = await provider.open('/d.txt', 'r');
  const asBuffer = await handle.readFile();
  assert.ok(Buffer.isBuffer(asBuffer));
  const asStringShorthand = await handle.readFile('utf8');
  assert.strictEqual(asStringShorthand, 'café');
  const asStringOption = await handle.readFile({ encoding: 'utf8' });
  assert.strictEqual(asStringOption, 'café');
  const asExplicitBuffer = await handle.readFile({ encoding: 'buffer' });
  assert.ok(Buffer.isBuffer(asExplicitBuffer));
  await handle.close();

  const writeHandle = await provider.open('/e.txt', 'w');
  await writeHandle.writeFile(Buffer.from('buffer-data'));
  await writeHandle.close();
  assert.strictEqual(await (await provider.open('/e.txt', 'r')).readFile('utf8'), 'buffer-data');

  const writeHandle2 = await provider.open('/f.txt', 'w');
  await writeHandle2.writeFile('string-data', { encoding: 'utf8' });
  await writeHandle2.close();
  assert.strictEqual(await (await provider.open('/f.txt', 'r')).readFile('utf8'), 'string-data');
})().then(common.mustCall());

(() => {
  const archive = buildArchiveSync([zlib.ZipEntry.createSync('g.txt', Buffer.from('sync-café'))]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);

  const handle = provider.openSync('/g.txt', 'r');
  assert.ok(Buffer.isBuffer(handle.readFileSync()));
  assert.strictEqual(handle.readFileSync('utf8'), 'sync-café');
  assert.strictEqual(handle.readFileSync({ encoding: 'utf8' }), 'sync-café');
  assert.ok(Buffer.isBuffer(handle.readFileSync({ encoding: 'buffer' })));
  handle.closeSync();

  const writeHandle = provider.openSync('/h.txt', 'w');
  writeHandle.writeFileSync(Buffer.from('sync-buffer-data'));
  writeHandle.closeSync();
  assert.strictEqual(provider.openSync('/h.txt', 'r').readFileSync('utf8'), 'sync-buffer-data');
})();

// --- rename() preserves the original compression method (methodOption) ----
(async () => {
  const archive = await buildArchive([
    await zlib.ZipEntry.create('store.bin', Buffer.from('a'.repeat(100)), { method: 'store' }),
    await zlib.ZipEntry.create('deflate.bin', Buffer.from('b'.repeat(100)), { method: 'deflate' }),
    await zlib.ZipEntry.create('zstd.bin', Buffer.from('c'.repeat(100)), { method: 'zstd' }),
  ]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);
  const archiveVfs = vfs.create(provider);

  const methodsBefore = new Map([
    ['store.bin', zip.get('store.bin').method],
    ['deflate.bin', zip.get('deflate.bin').method],
    ['zstd.bin', zip.get('zstd.bin').method],
  ]);

  await archiveVfs.promises.rename('/store.bin', '/store-renamed.bin');
  await archiveVfs.promises.rename('/deflate.bin', '/deflate-renamed.bin');
  await archiveVfs.promises.rename('/zstd.bin', '/zstd-renamed.bin');

  assert.strictEqual(zip.get('store-renamed.bin').method, methodsBefore.get('store.bin'));
  assert.strictEqual(zip.get('deflate-renamed.bin').method, methodsBefore.get('deflate.bin'));
  assert.strictEqual(zip.get('zstd-renamed.bin').method, methodsBefore.get('zstd.bin'));

  // Content must still round-trip correctly under the reproduced method.
  assert.strictEqual((await zip.get('store-renamed.bin').content()).toString(), 'a'.repeat(100));
  assert.strictEqual((await zip.get('deflate-renamed.bin').content()).toString(), 'b'.repeat(100));
  assert.strictEqual((await zip.get('zstd-renamed.bin').content()).toString(), 'c'.repeat(100));
})().then(common.mustCall());

(() => {
  const archive = buildArchiveSync([
    zlib.ZipEntry.createSync('store.bin', Buffer.from('a'.repeat(100)), { method: 'store' }),
    zlib.ZipEntry.createSync('zstd.bin', Buffer.from('c'.repeat(100)), { method: 'zstd' }),
  ]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);
  const archiveVfs = vfs.create(provider);

  const storeMethod = zip.get('store.bin').method;
  const zstdMethod = zip.get('zstd.bin').method;
  archiveVfs.renameSync('/store.bin', '/store-renamed.bin');
  archiveVfs.renameSync('/zstd.bin', '/zstd-renamed.bin');
  assert.strictEqual(zip.get('store-renamed.bin').method, storeMethod);
  assert.strictEqual(zip.get('zstd-renamed.bin').method, zstdMethod);
})();

// --- rename() with a missing source is rejected with ENOENT ---------------
(async () => {
  const archive = await buildArchive([await zlib.ZipEntry.create('only.txt', Buffer.from('x'))]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);
  const archiveVfs = vfs.create(provider);

  await assert.rejects(
    archiveVfs.promises.rename('/missing.txt', '/renamed.txt'),
    { code: 'ENOENT' },
  );
  assert.throws(
    () => archiveVfs.renameSync('/missing.txt', '/renamed.txt'),
    { code: 'ENOENT' },
  );
})().then(common.mustCall());

// --- rmdir() on a plain file is rejected with ENOTDIR ----------------------
// (called on the provider directly: the vfs router validates directory-ness
// itself before delegating for some operations, which would otherwise never
// exercise ZipProvider's own check).
(async () => {
  const archive = await buildArchive([await zlib.ZipEntry.create('file.txt', Buffer.from('x'))]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);

  await assert.rejects(provider.rmdir('/file.txt'), { code: 'ENOTDIR' });
  assert.throws(() => provider.rmdirSync('/file.txt'), { code: 'ENOTDIR' });
})().then(common.mustCall());

// --- open(): EEXIST/ENOENT/EISDIR-on-wrong-direction, called directly on
// the provider so the router can't short-circuit before delegating ---------
(async () => {
  const archive = await buildArchive([await zlib.ZipEntry.create('a.txt', Buffer.from('x'))]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);

  await assert.rejects(provider.open('/a.txt', 'wx'), { code: 'EEXIST' });
  assert.throws(() => provider.openSync('/a.txt', 'wx'), { code: 'EEXIST' });
  await assert.rejects(provider.open('/missing.txt', 'r'), { code: 'ENOENT' });
  assert.throws(() => provider.openSync('/missing.txt', 'r'), { code: 'ENOENT' });

  // A handle opened write-only can't be read from, and vice versa.
  const writeOnly = await provider.open('/w.txt', 'w');
  await assert.rejects(writeOnly.read(Buffer.alloc(1), 0, 1, 0), { code: 'EISDIR' });
  await writeOnly.close();
  const readOnly = await provider.open('/a.txt', 'r');
  await assert.rejects(readOnly.write(Buffer.alloc(1), 0, 1, 0), { code: 'EISDIR' });
  await readOnly.close();
})().then(common.mustCall());

// --- normalize(): a path without a leading slash is used as-is ------------
(async () => {
  const archive = await buildArchive([await zlib.ZipEntry.create('a.txt', Buffer.from('x'))]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);

  const stats = await provider.stat('a.txt');
  assert.strictEqual(stats.isFile(), true);
})().then(common.mustCall());

// --- mkdir(): both the with-options and no-options shapes, called
// directly on the provider ---------------------------------------------
(async () => {
  const archive = await buildArchive([]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);

  await provider.mkdir('/no-opts');
  assert.strictEqual((await provider.stat('/no-opts')).isDirectory(), true);
  await provider.mkdir('/with-opts', { mode: 0o700 });
  assert.strictEqual((await provider.stat('/with-opts')).isDirectory(), true);

  provider.mkdirSync('/no-opts-sync');
  assert.strictEqual(provider.statSync('/no-opts-sync').isDirectory(), true);
  provider.mkdirSync('/with-opts-sync', { mode: 0o700 });
  assert.strictEqual(provider.statSync('/with-opts-sync').isDirectory(), true);
})().then(common.mustCall());

// --- readdir() skips a directory's own explicit entry when listing it -----
(async () => {
  const archive = await buildArchive([
    await zlib.ZipEntry.create('dir/', Buffer.alloc(0)),
    await zlib.ZipEntry.create('dir/child.txt', Buffer.from('x')),
  ]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);
  const archiveVfs = vfs.create(provider);

  const entries = await archiveVfs.promises.readdir('/dir');
  assert.deepStrictEqual(entries, ['child.txt']);
})().then(common.mustCall());

// --- an entry not made by a Unix zip tool reports mode 0, so stat() and
// rename() fall back to their documented defaults --------------------------
(async () => {
  const archive = await buildArchive([
    await zlib.ZipEntry.create('foreign.txt', Buffer.from('x')),
    await zlib.ZipEntry.create('foreign-dir/', Buffer.alloc(0)),
  ]);
  const tampered = Buffer.from(archive);
  // The central header's "version made by" high byte selects the platform;
  // anything other than 3 (Unix) makes `mode` report 0 (see zip.js's
  // `CentralFileHeader.prototype.mode`).
  // The central directory follows *all* entries' local sections, and each
  // entry's central header follows the previous entries' central headers.
  const localSectionsLength = (30 + 'foreign.txt'.length + 'x'.length) +
    (30 + 'foreign-dir/'.length + 0);
  const fileHeaderStart = localSectionsLength;
  const dirHeaderStart = fileHeaderStart + (46 + 'foreign.txt'.length);
  const fileMadeByOffset = fileHeaderStart + 5;
  const dirMadeByOffset = dirHeaderStart + 5;
  assert.strictEqual(tampered[fileMadeByOffset], 3); // sanity: was Unix-made
  assert.strictEqual(tampered[dirMadeByOffset], 3);
  tampered[fileMadeByOffset] = 0; // MS-DOS/FAT
  tampered[dirMadeByOffset] = 0;

  const zip = new zlib.ZipBuffer(tampered);
  assert.strictEqual(zip.get('foreign.txt').mode, 0);
  assert.strictEqual(zip.get('foreign-dir/').mode, 0);
  const provider = new vfs.ZipProvider(zip);
  const archiveVfs = vfs.create(provider);

  // stat()'s `entry.mode || <default>` fallback, for both a file and an
  // (explicit) directory entry.
  const fileStats = await archiveVfs.promises.stat('/foreign.txt');
  assert.strictEqual(fileStats.mode & 0o777, 0o644);
  const dirStats = await archiveVfs.promises.stat('/foreign-dir');
  assert.strictEqual(dirStats.mode & 0o777, 0o755);
  assert.strictEqual(archiveVfs.statSync('/foreign.txt').mode & 0o777, 0o644);
  assert.strictEqual(archiveVfs.statSync('/foreign-dir').mode & 0o777, 0o755);

  // rename()'s `mode: entry.mode || undefined` fallback (undefined lets
  // ZipEntry.create() pick its own default mode instead of reproducing 0).
  await archiveVfs.promises.rename('/foreign.txt', '/renamed.txt');
  assert.strictEqual((await archiveVfs.promises.stat('/renamed.txt')).mode & 0o777, 0o644);
})().then(common.mustCall());

(() => {
  const archive = buildArchiveSync([zlib.ZipEntry.createSync('foreign.txt', Buffer.from('x'))]);
  const tampered = Buffer.from(archive);
  const centralHeaderStart = 30 + 'foreign.txt'.length + 'x'.length;
  const madeByOffset = centralHeaderStart + 5;
  tampered[madeByOffset] = 0;

  const zip = new zlib.ZipBuffer(tampered);
  const provider = new vfs.ZipProvider(zip);
  const archiveVfs = vfs.create(provider);

  archiveVfs.renameSync('/foreign.txt', '/renamed.txt');
  assert.strictEqual(archiveVfs.statSync('/renamed.txt').mode & 0o777, 0o644);
})();

// --- opening an explicit (empty) directory entry is rejected with EISDIR --
(async () => {
  const archive = await buildArchive([await zlib.ZipEntry.create('empty-dir/', Buffer.alloc(0))]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);

  await assert.rejects(provider.open('/empty-dir', 'r'), { code: 'EISDIR' });
  assert.throws(() => provider.openSync('/empty-dir', 'r'), { code: 'EISDIR' });
})().then(common.mustCall());

// --- readdir dedups a child directory reached through multiple entries ----
(async () => {
  const archive = await buildArchive([
    await zlib.ZipEntry.create('dir/one.txt', Buffer.from('1')),
    await zlib.ZipEntry.create('dir/two.txt', Buffer.from('2')),
  ]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);
  const archiveVfs = vfs.create(provider);

  // Both entries imply the same 'dir' child at the root; it must appear once.
  const rootEntries = await archiveVfs.promises.readdir('/');
  assert.deepStrictEqual(rootEntries, ['dir']);
  const dirEntries = await archiveVfs.promises.readdir('/dir');
  assert.deepStrictEqual(dirEntries.sort(), ['one.txt', 'two.txt']);
})().then(common.mustCall());

// --- open() with a write flag against a readonly (ZipFile) archive: EROFS --
(async () => {
  const archive = await buildArchive([await zlib.ZipEntry.create('a.txt', Buffer.from('x'))]);
  const filePath = path.join(tmpdir.path, 'vfs-archive-handle-readonly.zip');
  await fsPromises.writeFile(filePath, archive);
  const zip = await zlib.ZipFile.open(filePath);
  const provider = new vfs.ZipProvider(zip);

  await assert.rejects(provider.open('/new.txt', 'w'), { code: 'EROFS' });
  assert.throws(() => provider.openSync('/new.txt', 'w'), { code: 'EROFS' });

  await zip.close();
})().then(common.mustCall());

// --- ZipFile-backed writable archive: sync unlink/rmdir (deleteSync) ------
(async () => {
  const archive = await buildArchive([await zlib.ZipEntry.create('a.txt', Buffer.from('x'))]);
  const filePath = path.join(tmpdir.path, 'vfs-archive-handle-writable-sync.zip');
  await fsPromises.writeFile(filePath, archive);
  const zip = await zlib.ZipFile.open(filePath, { writable: true });
  const provider = new vfs.ZipProvider(zip);
  const archiveVfs = vfs.create(provider);

  archiveVfs.mkdirSync('/somedir');
  assert.strictEqual(archiveVfs.statSync('/somedir').isDirectory(), true);
  archiveVfs.rmdirSync('/somedir');
  assert.throws(() => archiveVfs.statSync('/somedir'), { code: 'ENOENT' });

  archiveVfs.unlinkSync('/a.txt');
  assert.throws(() => archiveVfs.statSync('/a.txt'), { code: 'ENOENT' });

  await zip.close();
})().then(common.mustCall());

// --- numeric open flags are normalized like node:fs (O_* constants) --------
(async () => {
  const { O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC } = fs.constants;
  const archive = await buildArchive([await zlib.ZipEntry.create('a.txt', Buffer.from('hello'))]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);

  // A numeric read-only flag reads the existing entry.
  const rh = await provider.open('/a.txt', O_RDONLY);
  assert.strictEqual(await rh.readFile('utf8'), 'hello');
  await rh.close();

  // A numeric write+create+truncate flag creates and commits a new entry.
  const wh = await provider.open('/num.txt', O_WRONLY | O_CREAT | O_TRUNC);
  await wh.writeFile('written via numeric flags');
  await wh.close();
  assert.strictEqual(await provider.readFile('/num.txt', 'utf8'), 'written via numeric flags');

  // The synchronous surface normalizes numeric flags identically.
  const sh = provider.openSync('/a.txt', O_RDONLY);
  assert.strictEqual(sh.readFileSync('utf8'), 'hello');
  sh.closeSync();
  const swh = provider.openSync('/num-sync.txt', O_WRONLY | O_CREAT | O_TRUNC);
  swh.writeFileSync('sync numeric');
  swh.closeSync();
  assert.strictEqual(provider.readFileSync('/num-sync.txt', 'utf8'), 'sync numeric');
})().then(common.mustCall());

// --- readonly archive: any writable open flag (including 'r+' and numeric
// O_RDWR) is rejected with EROFS, matching MemoryProvider/real fs -----------
(async () => {
  const archive = await buildArchive([await zlib.ZipEntry.create('a.txt', Buffer.from('x'))]);
  const filePath = path.join(tmpdir.path, 'vfs-archive-handle-rplus-readonly.zip');
  await fsPromises.writeFile(filePath, archive);
  const zip = await zlib.ZipFile.open(filePath); // read-only
  const provider = new vfs.ZipProvider(zip);

  // 'r+' opens for writing, so it is refused even though it also reads.
  await assert.rejects(provider.open('/a.txt', 'r+'), { code: 'EROFS' });
  assert.throws(() => provider.openSync('/a.txt', 'r+'), { code: 'EROFS' });

  // Numeric O_RDWR is likewise a writable flag.
  await assert.rejects(provider.open('/a.txt', fs.constants.O_RDWR), { code: 'EROFS' });
  assert.throws(() => provider.openSync('/a.txt', fs.constants.O_RDWR), { code: 'EROFS' });

  // Plain read access is still allowed.
  const h = await provider.open('/a.txt', 'r');
  assert.strictEqual(await h.readFile('utf8'), 'x');
  await h.close();

  await zip.close();
})().then(common.mustCall());

// --- mkdir({ recursive: true }) over an existing FILE still throws EEXIST;
// over an existing DIRECTORY it stays a no-op -------------------------------
(async () => {
  const archive = await buildArchive([await zlib.ZipEntry.create('a.txt', Buffer.from('x'))]);
  const zip = new zlib.ZipBuffer(archive);
  const provider = new vfs.ZipProvider(zip);

  await assert.rejects(provider.mkdir('/a.txt', { recursive: true }), { code: 'EEXIST' });
  assert.throws(() => provider.mkdirSync('/a.txt', { recursive: true }), { code: 'EEXIST' });

  // Recursive mkdir over an existing directory remains a no-op (no throw).
  await provider.mkdir('/d');
  await provider.mkdir('/d', { recursive: true });
  provider.mkdirSync('/d', { recursive: true });
  assert.strictEqual((await provider.stat('/d')).isDirectory(), true);
})().then(common.mustCall());
