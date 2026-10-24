// Flags: --disable-warning=ExperimentalWarning
'use strict';

// ZipProvider directory renames must follow the same rules as the default
// provider (see test-vfs-rename.js): a directory cannot replace a non-empty
// directory (ENOTEMPTY) or a non-directory (ENOTDIR), and a rename that fails
// part-way must not leave the archive modified.
// Refs: https://github.com/nodejs/node/issues/66972

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const vfs = require('node:vfs');

tmpdir.refresh();

const CORRUPT_PAYLOAD = Buffer.from('payload whose CRC-32 will no longer match');

async function buildArchive(entries) {
  const members = [];
  for (const [name, content, options] of entries) {
    members.push(await zlib.ZipEntry.create(name, Buffer.from(content), options));
  }
  const chunks = [];
  for await (const chunk of zlib.createZipArchive(members)) chunks.push(chunk);
  return Buffer.concat(chunks);
}

// Flips one byte of a stored (uncompressed) member so that reading it fails
// its CRC-32 check.
function corrupt(archive) {
  const offset = archive.indexOf(CORRUPT_PAYLOAD);
  assert.ok(offset > 0, 'corrupt member must be stored uncompressed');
  archive[offset] ^= 0xff;
  return archive;
}

function sortedKeys(source) {
  return [...source.keys()].sort();
}

// Run every case through both the synchronous and promise-based API.
const renamers = [
  ['renameSync', async (v, from, to) => v.renameSync(from, to)],
  ['promises.rename', (v, from, to) => v.promises.rename(from, to)],
];

(async () => {
  for (const [label, rename] of renamers) {
    // Directory onto a non-empty directory throws ENOTEMPTY and changes nothing.
    {
      const zip = new zlib.ZipBuffer(await buildArchive([
        ['src/file.txt', 'Original A'],
        ['dst/file.txt', 'Original B'],
        ['dst/keep.txt', 'Keep this file'],
      ]));
      const before = sortedKeys(zip);
      const myVfs = vfs.create(new vfs.ZipProvider(zip));
      await assert.rejects(rename(myVfs, '/src', '/dst'), { code: 'ENOTEMPTY' }, label);
      assert.deepStrictEqual(sortedKeys(zip), before);
      assert.strictEqual(myVfs.readFileSync('/dst/file.txt', 'utf8'), 'Original B');
      assert.strictEqual(myVfs.readFileSync('/src/file.txt', 'utf8'), 'Original A');
    }

    // Same, when the destination is an explicit directory entry.
    {
      const zip = new zlib.ZipBuffer(await buildArchive([
        ['src/file.txt', 'Original A'],
        ['dst/', ''],
        ['dst/keep.txt', 'Keep this file'],
      ]));
      const before = sortedKeys(zip);
      const myVfs = vfs.create(new vfs.ZipProvider(zip));
      await assert.rejects(rename(myVfs, '/src', '/dst'), { code: 'ENOTEMPTY' }, label);
      assert.deepStrictEqual(sortedKeys(zip), before);
    }

    // Directory onto a regular file throws ENOTDIR and changes nothing.
    {
      const zip = new zlib.ZipBuffer(await buildArchive([
        ['src/child.txt', 'CHILD CONTENT'],
        ['target', 'ORIGINAL FILE'],
      ]));
      const before = sortedKeys(zip);
      const myVfs = vfs.create(new vfs.ZipProvider(zip));
      await assert.rejects(rename(myVfs, '/src', '/target'), { code: 'ENOTDIR' }, label);
      assert.deepStrictEqual(sortedKeys(zip), before);
      assert.strictEqual(myVfs.readFileSync('/target', 'utf8'), 'ORIGINAL FILE');
    }

    // A source member that cannot be read fails the rename before anything
    // is written, whether or not the destination already exists.
    {
      const zip = new zlib.ZipBuffer(corrupt(await buildArchive([
        ['src/01-good.txt', 'FIRST'],
        ['src/02-bad.bin', CORRUPT_PAYLOAD, { method: 'store' }],
      ])));
      const before = sortedKeys(zip);
      const myVfs = vfs.create(new vfs.ZipProvider(zip));
      await assert.rejects(rename(myVfs, '/src', '/moved'),
                           { code: 'ERR_ZIP_ENTRY_CORRUPT' }, label);
      assert.deepStrictEqual(sortedKeys(zip), before);
    }

    // Renames that must keep working.
    {
      const zip = new zlib.ZipBuffer(await buildArchive([
        ['src/file.txt', 'A'],
        ['empty/', ''],
        ['f.txt', 'x'],
      ]));
      const myVfs = vfs.create(new vfs.ZipProvider(zip));

      // Directory onto an empty directory.
      await rename(myVfs, '/src', '/empty');
      assert.strictEqual(myVfs.existsSync('/src'), false);
      assert.strictEqual(myVfs.readFileSync('/empty/file.txt', 'utf8'), 'A');

      // Directory to a new name.
      await rename(myVfs, '/empty', '/moved');
      assert.strictEqual(myVfs.existsSync('/empty'), false);
      assert.strictEqual(myVfs.readFileSync('/moved/file.txt', 'utf8'), 'A');

      // File onto a directory still throws EISDIR; directory into its own
      // descendant still throws EINVAL.
      await assert.rejects(rename(myVfs, '/f.txt', '/moved'), { code: 'EISDIR' }, label);
      await assert.rejects(rename(myVfs, '/moved', '/moved/sub'), { code: 'EINVAL' }, label);
    }

    // Disk-backed archive: a failed rename leaves the file on disk unchanged.
    {
      const file = path.join(tmpdir.path, `rename-${label}.zip`);
      fs.writeFileSync(file, corrupt(await buildArchive([
        ['dst/01-good.txt', 'ORIGINAL DESTINATION'],
        ['src/01-good.txt', 'REPLACEMENT DATA'],
        ['src/02-bad.bin', CORRUPT_PAYLOAD, { method: 'store' }],
      ])));

      const zip = zlib.ZipFile.openSync(file, { writable: true });
      const myVfs = vfs.create(new vfs.ZipProvider(zip));
      await assert.rejects(rename(myVfs, '/src', '/dst'), { code: 'ENOTEMPTY' }, label);
      await assert.rejects(rename(myVfs, '/src', '/moved'),
                           { code: 'ERR_ZIP_ENTRY_CORRUPT' }, label);
      zip.closeSync();

      const reopened = zlib.ZipFile.openSync(file);
      assert.deepStrictEqual(sortedKeys(reopened), [
        'dst/01-good.txt', 'src/01-good.txt', 'src/02-bad.bin',
      ]);
      assert.strictEqual(
        vfs.create(new vfs.ZipProvider(reopened)).readFileSync('/dst/01-good.txt', 'utf8'),
        'ORIGINAL DESTINATION');
      reopened.closeSync();
    }
  }
})().then(common.mustCall());
