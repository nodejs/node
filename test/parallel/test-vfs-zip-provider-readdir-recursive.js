// Flags: --experimental-vfs
'use strict';

// A recursive readdir() of a ZipProvider lists every member below the
// directory, together with every directory their paths pass through, whether
// or not the archive holds an entry for it.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const vfs = require('node:vfs');

async function buildArchive(entries) {
  const chunks = [];
  for await (const chunk of zlib.createZipArchive(entries)) chunks.push(chunk);
  return Buffer.concat(chunks);
}

(async () => {
  const archive = await buildArchive([
    await zlib.ZipEntry.create('top.txt', Buffer.from('top')),
    // Directories only implied by a member's path, several levels deep.
    await zlib.ZipEntry.create('a/b/c/deep.txt', Buffer.from('deep')),
    // An explicit directory entry, before and after members inside it.
    await zlib.ZipEntry.create('a/b/', Buffer.alloc(0)),
    await zlib.ZipEntry.create('a/b/sibling.txt', Buffer.from('sibling')),
    await zlib.ZipEntry.create('empty/', Buffer.alloc(0)),
  ]);
  const provider = new vfs.ZipProvider(new zlib.ZipBuffer(archive));

  const all = [
    'a', 'a/b', 'a/b/c', 'a/b/c/deep.txt', 'a/b/sibling.txt', 'empty', 'top.txt',
  ];
  const dirs = new Set(['a', 'a/b', 'a/b/c', 'empty']);

  // Each directory is listed once, whether implied, explicit, or both.
  assert.deepStrictEqual(provider.readdirSync('/', { recursive: true }).sort(), all);
  assert.deepStrictEqual((await provider.readdir('/', { recursive: true })).sort(), all);

  const dirents = provider.readdirSync('/', { recursive: true, withFileTypes: true });
  assert.deepStrictEqual(dirents.map((d) => d.name).sort(), all);
  for (const dirent of dirents) {
    assert.strictEqual(dirent.isDirectory(), dirs.has(dirent.name), dirent.name);
    assert.strictEqual(dirent.isFile(), !dirs.has(dirent.name), dirent.name);
  }

  // Listing a subdirectory yields paths relative to it.
  assert.deepStrictEqual(provider.readdirSync('/a/b', { recursive: true }).sort(),
                         ['c', 'c/deep.txt', 'sibling.txt']);
  assert.deepStrictEqual(provider.readdirSync('/empty', { recursive: true }), []);
  assert.throws(() => provider.readdirSync('/top.txt', { recursive: true }),
                { code: 'ENOTDIR' });
  assert.throws(() => provider.readdirSync('/missing', { recursive: true }),
                { code: 'ENOENT' });

  // A non-recursive listing is unchanged.
  assert.deepStrictEqual(provider.readdirSync('/').sort(), ['a', 'empty', 'top.txt']);
  assert.deepStrictEqual(provider.readdirSync('/a/b').sort(), ['c', 'sibling.txt']);

  // A name that is both a member and a directory is listed as a directory.
  {
    const clash = new vfs.ZipProvider(new zlib.ZipBuffer(await buildArchive([
      await zlib.ZipEntry.create('x', Buffer.from('file')),
      await zlib.ZipEntry.create('x/y.txt', Buffer.from('nested')),
    ])));
    const entries = clash.readdirSync('/', { recursive: true, withFileTypes: true });
    assert.deepStrictEqual(entries.map((d) => [d.name, d.isDirectory()]).sort(),
                           [['x', true], ['x/y.txt', false]]);
  }

  // Through node:fs, each Dirent reports its own parent directory.
  {
    const archiveVfs = vfs.create(provider);
    const mountPoint = archiveVfs.mount();
    const listed = fs.readdirSync(mountPoint, { recursive: true, withFileTypes: true })
      .map((d) => path.join(d.parentPath, d.name)).sort();
    assert.deepStrictEqual(listed, all.map((p) => path.join(mountPoint, p)).sort());
    assert.deepStrictEqual(fs.readdirSync(mountPoint, { recursive: true }).sort(), all);
    archiveVfs.unmount();
  }
})().then(common.mustCall());
