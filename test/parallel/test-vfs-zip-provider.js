// Flags: --experimental-vfs
'use strict';

// Exercises ZipProvider (node:vfs backed by node:zlib's ZipBuffer/
// ZipFile): construction validation, readonly reflecting the archive's own
// writability, stat/readdir over explicit and implicit directories, and the
// full async and synchronous CRUD surface, against both a ZipBuffer and a
// ZipFile (opened both via open() and openSync()) source.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const path = require('path');
const fsPromises = require('fs/promises');
const zlib = require('zlib');
const vfs = require('node:vfs');

tmpdir.refresh();

async function buildArchive(entries, comment) {
  const chunks = [];
  for await (const chunk of zlib.createZipArchive(entries, comment)) chunks.push(chunk);
  return Buffer.concat(chunks);
}

(async () => {
  // Construction validation.
  assert.throws(() => new vfs.ZipProvider({}), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => new vfs.ZipProvider(null), { code: 'ERR_INVALID_ARG_TYPE' });

  // --- ZipBuffer-backed: always writable ------------------------------------
  {
    const archive = await buildArchive([
      await zlib.ZipEntry.create('a.txt', Buffer.from('hello')),
      await zlib.ZipEntry.create('dir/b.txt', Buffer.from('nested')),
      await zlib.ZipEntry.create('empty-dir/', Buffer.alloc(0)),
    ]);
    const zip = new zlib.ZipBuffer(archive);
    const provider = new vfs.ZipProvider(zip);
    assert.strictEqual(provider.readonly, false);
    assert.strictEqual(provider.supportsSymlinks, false);
    assert.strictEqual(provider.supportsWatch, false);

    const archiveVfs = vfs.create(provider);

    // stat: file, implicit directory, explicit directory, root.
    const fileStat = await archiveVfs.promises.stat('/a.txt');
    assert.strictEqual(fileStat.isFile(), true);
    assert.strictEqual(fileStat.size, 5);

    const implicitDirStat = await archiveVfs.promises.stat('/dir');
    assert.strictEqual(implicitDirStat.isDirectory(), true);

    const explicitDirStat = await archiveVfs.promises.stat('/empty-dir');
    assert.strictEqual(explicitDirStat.isDirectory(), true);

    const rootStat = await archiveVfs.promises.stat('/');
    assert.strictEqual(rootStat.isDirectory(), true);

    await assert.rejects(archiveVfs.promises.stat('/missing.txt'), { code: 'ENOENT' });

    // readdir: root lists both files and directories, deduped.
    const rootEntries = await archiveVfs.promises.readdir('/');
    assert.deepStrictEqual(rootEntries.sort(), ['a.txt', 'dir', 'empty-dir']);

    const dirEntries = await archiveVfs.promises.readdir('/dir');
    assert.deepStrictEqual(dirEntries, ['b.txt']);

    const withTypes = await archiveVfs.promises.readdir('/', { withFileTypes: true });
    const byName = new Map(withTypes.map((d) => [d.name, d]));
    assert.strictEqual(byName.get('a.txt').isFile(), true);
    assert.strictEqual(byName.get('dir').isDirectory(), true);

    await assert.rejects(archiveVfs.promises.readdir('/a.txt'), { code: 'ENOTDIR' });
    await assert.rejects(
      archiveVfs.promises.readdir('/', { recursive: true }),
      { code: 'ERR_METHOD_NOT_IMPLEMENTED' },
    );

    // readFile / writeFile round trip (new file).
    assert.strictEqual(await archiveVfs.promises.readFile('/a.txt', 'utf8'), 'hello');
    await archiveVfs.promises.writeFile('/new.txt', 'brand new');
    assert.strictEqual(await archiveVfs.promises.readFile('/new.txt', 'utf8'), 'brand new');
    assert.strictEqual(zip.has('new.txt'), true);

    // Overwriting an existing file.
    await archiveVfs.promises.writeFile('/a.txt', 'overwritten');
    assert.strictEqual(await archiveVfs.promises.readFile('/a.txt', 'utf8'), 'overwritten');

    // appendFile.
    await archiveVfs.promises.writeFile('/append.txt', 'ab');
    await archiveVfs.promises.appendFile('/append.txt', 'cd');
    assert.strictEqual(await archiveVfs.promises.readFile('/append.txt', 'utf8'), 'abcd');

    // mkdir + rmdir.
    await archiveVfs.promises.mkdir('/newdir');
    assert.strictEqual((await archiveVfs.promises.stat('/newdir')).isDirectory(), true);
    await assert.rejects(archiveVfs.promises.mkdir('/newdir'), { code: 'EEXIST' });
    await archiveVfs.promises.mkdir('/newdir', { recursive: true }); // No throw, already exists
    await archiveVfs.promises.rmdir('/newdir');
    await assert.rejects(archiveVfs.promises.stat('/newdir'), { code: 'ENOENT' });

    // Rmdir refuses a non-empty directory.
    await assert.rejects(archiveVfs.promises.rmdir('/dir'), { code: 'ENOTEMPTY' });

    // unlink.
    await archiveVfs.promises.unlink('/append.txt');
    await assert.rejects(archiveVfs.promises.stat('/append.txt'), { code: 'ENOENT' });
    await assert.rejects(archiveVfs.promises.unlink('/missing.txt'), { code: 'ENOENT' });
    await assert.rejects(archiveVfs.promises.unlink('/dir'), { code: 'EISDIR' });

    // rename.
    await archiveVfs.promises.writeFile('/rename-me.txt', 'content');
    await archiveVfs.promises.rename('/rename-me.txt', '/renamed.txt');
    assert.strictEqual(zip.has('rename-me.txt'), false);
    assert.strictEqual(await archiveVfs.promises.readFile('/renamed.txt', 'utf8'), 'content');

    // open() flag semantics.
    await assert.rejects(archiveVfs.promises.open('/does-not-exist.txt', 'r'), { code: 'ENOENT' });
    await assert.rejects(archiveVfs.promises.open('/a.txt', 'wx'), { code: 'EEXIST' });
    await assert.rejects(archiveVfs.promises.open('/dir', 'r'), { code: 'EISDIR' });
  }

  // --- ZipFile-backed, read-only: writes rejected with EROFS ----------------
  {
    const archive = await buildArchive([await zlib.ZipEntry.create('a.txt', Buffer.from('x'))]);
    const filePath = path.join(tmpdir.path, 'vfs-archive-readonly.zip');
    await fsPromises.writeFile(filePath, archive);
    const zip = await zlib.ZipFile.open(filePath);
    const provider = new vfs.ZipProvider(zip);
    assert.strictEqual(provider.readonly, true);
    const archiveVfs = vfs.create(provider);

    assert.strictEqual(await archiveVfs.promises.readFile('/a.txt', 'utf8'), 'x');
    await assert.rejects(archiveVfs.promises.writeFile('/new.txt', 'y'), { code: 'EROFS' });
    await assert.rejects(archiveVfs.promises.unlink('/a.txt'), { code: 'EROFS' });
    await assert.rejects(archiveVfs.promises.mkdir('/newdir'), { code: 'EROFS' });
    await assert.rejects(archiveVfs.promises.rmdir('/newdir'), { code: 'EROFS' });
    await assert.rejects(archiveVfs.promises.rename('/a.txt', '/b.txt'), { code: 'EROFS' });

    // The synchronous surface rejects the same way.
    assert.strictEqual(archiveVfs.readFileSync('/a.txt', 'utf8'), 'x');
    assert.throws(() => archiveVfs.writeFileSync('/new.txt', 'y'), { code: 'EROFS' });
    assert.throws(() => archiveVfs.unlinkSync('/a.txt'), { code: 'EROFS' });
    assert.throws(() => archiveVfs.mkdirSync('/newdir'), { code: 'EROFS' });
    assert.throws(() => archiveVfs.rmdirSync('/newdir'), { code: 'EROFS' });
    assert.throws(() => archiveVfs.renameSync('/a.txt', '/b.txt'), { code: 'EROFS' });

    await zip.close();
  }

  // --- ZipFile-backed, opened writable: mutations persist to disk ----------
  {
    const archive = await buildArchive([await zlib.ZipEntry.create('a.txt', Buffer.from('x'))]);
    const filePath = path.join(tmpdir.path, 'vfs-archive-writable.zip');
    await fsPromises.writeFile(filePath, archive);
    const zip = await zlib.ZipFile.open(filePath, { writable: true });
    const provider = new vfs.ZipProvider(zip);
    assert.strictEqual(provider.readonly, false);
    const archiveVfs = vfs.create(provider);

    await archiveVfs.promises.writeFile('/b.txt', 'new content');
    await zip.close();

    const reopened = await zlib.ZipFile.open(filePath);
    assert.strictEqual((await (await reopened.get('b.txt')).content()).toString(), 'new content');
    await reopened.close();
  }

  // --- ZipBuffer-backed, fully synchronous CRUD ------------------------------
  {
    const archive = await buildArchive([
      await zlib.ZipEntry.create('a.txt', Buffer.from('hello')),
      await zlib.ZipEntry.create('dir/b.txt', Buffer.from('nested')),
    ]);
    const zip = new zlib.ZipBuffer(archive);
    const provider = new vfs.ZipProvider(zip);
    const archiveVfs = vfs.create(provider);

    // stat/readdir.
    assert.strictEqual(archiveVfs.statSync('/a.txt').isFile(), true);
    assert.strictEqual(archiveVfs.statSync('/dir').isDirectory(), true);
    assert.throws(() => archiveVfs.statSync('/missing.txt'), { code: 'ENOENT' });
    assert.deepStrictEqual(archiveVfs.readdirSync('/').sort(), ['a.txt', 'dir']);
    assert.throws(() => archiveVfs.readdirSync('/a.txt'), { code: 'ENOTDIR' });
    assert.throws(
      () => archiveVfs.readdirSync('/', { recursive: true }),
      { code: 'ERR_METHOD_NOT_IMPLEMENTED' },
    );

    // readFile/writeFile/appendFile round trip.
    assert.strictEqual(archiveVfs.readFileSync('/a.txt', 'utf8'), 'hello');
    archiveVfs.writeFileSync('/new.txt', 'brand new');
    assert.strictEqual(archiveVfs.readFileSync('/new.txt', 'utf8'), 'brand new');
    archiveVfs.appendFileSync('/new.txt', '!');
    assert.strictEqual(archiveVfs.readFileSync('/new.txt', 'utf8'), 'brand new!');

    // mkdir/rmdir.
    archiveVfs.mkdirSync('/newdir');
    assert.strictEqual(archiveVfs.statSync('/newdir').isDirectory(), true);
    assert.throws(() => archiveVfs.mkdirSync('/newdir'), { code: 'EEXIST' });
    archiveVfs.mkdirSync('/newdir', { recursive: true }); // No throw, already exists.
    archiveVfs.rmdirSync('/newdir');
    assert.throws(() => archiveVfs.statSync('/newdir'), { code: 'ENOENT' });
    assert.throws(() => archiveVfs.rmdirSync('/dir'), { code: 'ENOTEMPTY' });

    // unlink.
    archiveVfs.unlinkSync('/new.txt');
    assert.throws(() => archiveVfs.statSync('/new.txt'), { code: 'ENOENT' });
    assert.throws(() => archiveVfs.unlinkSync('/missing.txt'), { code: 'ENOENT' });
    assert.throws(() => archiveVfs.unlinkSync('/dir'), { code: 'EISDIR' });

    // rename.
    archiveVfs.writeFileSync('/rename-me.txt', 'content');
    archiveVfs.renameSync('/rename-me.txt', '/renamed.txt');
    assert.strictEqual(zip.has('rename-me.txt'), false);
    assert.strictEqual(archiveVfs.readFileSync('/renamed.txt', 'utf8'), 'content');

    // open() flag semantics.
    assert.throws(() => archiveVfs.openSync('/does-not-exist.txt', 'r'), { code: 'ENOENT' });
    assert.throws(() => archiveVfs.openSync('/a.txt', 'wx'), { code: 'EEXIST' });
    assert.throws(() => archiveVfs.openSync('/dir', 'r'), { code: 'EISDIR' });
  }

  // --- ZipFile-backed via openSync: sync-only round trip on disk -----------
  {
    const archive = await buildArchive([await zlib.ZipEntry.create('a.txt', Buffer.from('x'))]);
    const filePath = path.join(tmpdir.path, 'vfs-archive-opensync.zip');
    await fsPromises.writeFile(filePath, archive);
    const zip = zlib.ZipFile.openSync(filePath, { writable: true });
    const provider = new vfs.ZipProvider(zip);
    assert.strictEqual(provider.readonly, false);
    const archiveVfs = vfs.create(provider);

    assert.strictEqual(archiveVfs.readFileSync('/a.txt', 'utf8'), 'x');
    archiveVfs.writeFileSync('/b.txt', 'new content');
    zip.closeSync();

    const reopened = zlib.ZipFile.openSync(filePath);
    assert.strictEqual(reopened.getSync('b.txt').contentSync().toString(), 'new content');
    reopened.closeSync();
  }
})().then(common.mustCall());
