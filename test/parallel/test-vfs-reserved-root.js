// Flags: --experimental-vfs
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const vfs = require('node:vfs');

const root = path.join(os.devNull, 'vfs');

const a = vfs.create();
a.mkdirSync('/dir');
a.writeFileSync('/dir/file.txt', 'from a');
a.writeFileSync('/mod.js', 'module.exports = __filename;');
a.writeFileSync('/mod.mjs', 'export default import.meta.url;');
const b = vfs.create();
b.writeFileSync('/file.txt', 'from b');
const unnamed = vfs.create();

const mountA = a.mount('a');
const mountB = b.mount('b');
const mountUnnamed = unnamed.mount();
const idA = path.basename(mountA);
const idB = path.basename(mountB);
const idUnnamed = path.basename(mountUnnamed);
const layerIds = [idA, idB, idUnnamed].sort((x, y) => x - y);
const linkA = path.join(root, 'a');
const linkB = path.join(root, 'b');

// The root lists every layer by id, then every name.
{
  const expected = [...layerIds, 'a', 'b'];
  assert.deepStrictEqual(fs.readdirSync(root), expected);
  assert.deepStrictEqual(fs.readdirSync(`${root}${path.sep}`), expected);
  assert.deepStrictEqual(fs.readdirSync(root, { encoding: 'buffer' }),
                         expected.map((name) => Buffer.from(name)));

  const dirents = fs.readdirSync(root, { withFileTypes: true });
  assert.deepStrictEqual(dirents.map((d) => d.name), expected);
  for (const dirent of dirents) {
    assert.strictEqual(dirent.parentPath, root);
    const isLink = dirent.name === 'a' || dirent.name === 'b';
    assert.strictEqual(dirent.isSymbolicLink(), isLink, dirent.name);
    assert.strictEqual(dirent.isDirectory(), !isLink, dirent.name);
  }

  const dir = fs.opendirSync(root);
  const names = [];
  let dirent;
  while ((dirent = dir.readSync()) !== null) names.push(dirent.name);
  dir.closeSync();
  assert.deepStrictEqual(names, expected);

  fs.readdir(root, common.mustSucceed((result) => {
    assert.deepStrictEqual(result, expected);
  }));
  fs.promises.readdir(root).then(common.mustCall((result) => {
    assert.deepStrictEqual(result, expected);
  }));
}

// A recursive listing descends into the layers but not through the names.
{
  const expected = [
    ...layerIds, 'a', 'b',
    ...layerIds.flatMap((id) => {
      if (id === idA) return ['dir', 'mod.js', 'mod.mjs', 'dir/file.txt'].map((n) => `${id}/${n}`);
      if (id === idB) return [`${id}/file.txt`];
      return [];
    }),
  ];
  assert.deepStrictEqual(fs.readdirSync(root, { recursive: true }).sort(),
                         expected.sort());

  const dirents = fs.readdirSync(root, { recursive: true, withFileTypes: true });
  const file = dirents.find((d) => d.name === 'file.txt' &&
                                   d.parentPath === path.join(mountA, 'dir'));
  assert.ok(file?.isFile());
}

// The root is a directory, and a name is a symbolic link to its layer.
{
  assert.ok(fs.statSync(root).isDirectory());
  assert.ok(fs.lstatSync(root).isDirectory());
  assert.ok(fs.statSync(root, { bigint: true }).isDirectory());
  assert.ok(fs.existsSync(root));
  fs.accessSync(root, fs.constants.R_OK);
  assert.strictEqual(fs.realpathSync(root), root);

  assert.ok(fs.lstatSync(linkA).isSymbolicLink());
  assert.ok(fs.statSync(linkA).isDirectory());
  assert.strictEqual(fs.readlinkSync(linkA), idA);
  assert.deepStrictEqual(fs.readlinkSync(linkA, { encoding: 'buffer' }),
                         Buffer.from(idA));
  assert.strictEqual(fs.realpathSync(linkA), mountA);
  assert.strictEqual(fs.realpathSync.native(linkA), mountA);
  assert.throws(() => fs.readlinkSync(root), { code: 'EINVAL' });

  assert.ok(fs.lstatSync(path.join(root, idA)).isDirectory());

  fs.lstat(linkA, common.mustSucceed((stats) => {
    assert.ok(stats.isSymbolicLink());
  }));
  fs.readlink(linkA, common.mustSucceed((target) => {
    assert.strictEqual(target, idA);
  }));
  fs.realpath(linkA, common.mustSucceed((real) => {
    assert.strictEqual(real, mountA);
  }));
  fs.promises.lstat(linkB).then(common.mustCall((stats) => {
    assert.ok(stats.isSymbolicLink());
  }));
  fs.promises.readlink(linkB).then(common.mustCall((target) => {
    assert.strictEqual(target, idB);
  }));
  fs.promises.realpath(path.join(linkB, 'file.txt')).then(common.mustCall((real) => {
    assert.strictEqual(real, path.join(mountB, 'file.txt'));
  }));
}

// Anything else under the root does not exist.
{
  for (const missing of ['missing', 'missing/deeper', `${idA}0`, `0${idA}`,
                         `${idA}.js`, 'a.js']) {
    const p = path.join(root, missing);
    assert.strictEqual(fs.existsSync(p), false, p);
    assert.throws(() => fs.statSync(p), { code: 'ENOENT' }, p);
    assert.throws(() => fs.lstatSync(p), { code: 'ENOENT' }, p);
    assert.throws(() => fs.readdirSync(p), { code: 'ENOENT' }, p);
    assert.throws(() => fs.readFileSync(p), { code: 'ENOENT' }, p);
    assert.throws(() => fs.realpathSync(p), { code: 'ENOENT' }, p);
    assert.strictEqual(fs.statSync(p, { throwIfNoEntry: false }), undefined);
  }
}

// Paths through a name reach the layer.
{
  assert.strictEqual(fs.readFileSync(path.join(linkA, 'dir', 'file.txt'), 'utf8'),
                     'from a');
  assert.deepStrictEqual(fs.readdirSync(linkA).sort(), ['dir', 'mod.js', 'mod.mjs']);
  assert.deepStrictEqual(fs.readdirSync(path.join(linkA, 'dir')), ['file.txt']);
  assert.strictEqual(fs.realpathSync(path.join(linkA, 'dir', 'file.txt')),
                     path.join(mountA, 'dir', 'file.txt'));
  assert.ok(fs.statSync(path.join(linkA, 'dir')).isDirectory());
  assert.ok(fs.lstatSync(path.join(linkA, 'dir')).isDirectory());

  const fd = fs.openSync(path.join(linkB, 'file.txt'));
  assert.strictEqual(fs.readFileSync(fd, 'utf8'), 'from b');
  fs.closeSync(fd);

  // Writes through a name land in the layer.
  fs.writeFileSync(path.join(linkA, 'new.txt'), 'written');
  assert.strictEqual(a.readFileSync(path.join(mountA, 'new.txt'), 'utf8'), 'written');
  fs.renameSync(path.join(linkA, 'new.txt'), path.join(mountA, 'renamed.txt'));
  assert.strictEqual(a.readFileSync(path.join(mountA, 'renamed.txt'), 'utf8'), 'written');
  fs.mkdirSync(path.join(linkA, 'sub'));
  fs.rmSync(path.join(linkA, 'sub'), { recursive: true });
  fs.unlinkSync(path.join(linkA, 'renamed.txt'));
  assert.strictEqual(a.existsSync(path.join(mountA, 'renamed.txt')), false);
  // A layer is a file system of its own.
  assert.throws(() => fs.renameSync(path.join(linkA, 'mod.js'),
                                    path.join(linkB, 'mod.js')),
                { code: 'EXDEV' });

  fs.readFile(path.join(linkB, 'file.txt'), 'utf8', common.mustSucceed((data) => {
    assert.strictEqual(data, 'from b');
  }));
  fs.promises.readFile(path.join(linkB, 'file.txt'), 'utf8').then(common.mustCall((data) => {
    assert.strictEqual(data, 'from b');
  }));
}

// The loader resolves a name like any symbolic link.
{
  assert.strictEqual(require(path.join(linkA, 'mod.js')), path.join(mountA, 'mod.js'));
  assert.strictEqual(require(path.join(linkA, 'mod')), path.join(mountA, 'mod.js'));
  import(pathToFileURL(path.join(linkA, 'mod.mjs')).href).then(common.mustCall((ns) => {
    assert.strictEqual(ns.default, pathToFileURL(path.join(mountA, 'mod.mjs')).href);
  }));
}

// The root and the names in it are read-only.
{
  const erofs = { code: 'EROFS' };
  const newPath = path.join(root, 'new');
  assert.throws(() => fs.writeFileSync(newPath, ''), erofs);
  assert.throws(() => fs.mkdirSync(newPath), erofs);
  assert.throws(() => fs.mkdirSync(path.join(newPath, 'deeper'), { recursive: true }), erofs);
  assert.throws(() => fs.mkdtempSync(path.join(root, 'tmp-')), erofs);
  assert.throws(() => fs.mkdtempSync(linkA), erofs);
  assert.throws(() => fs.symlinkSync(idA, newPath), erofs);
  assert.throws(() => fs.openSync(newPath, 'w'), erofs);
  assert.throws(() => fs.openSync(root, 'r+'), erofs);
  assert.throws(() => fs.openSync(root, 'r'), { code: 'EISDIR' });

  assert.throws(() => fs.unlinkSync(linkA), erofs);
  assert.throws(() => fs.rmSync(linkA), erofs);
  assert.throws(() => fs.rmSync(linkA, { recursive: true, force: true }), erofs);
  assert.throws(() => fs.rmSync(root, { recursive: true, force: true }), erofs);
  // Like the mount points in it, the root cannot be removed.
  assert.throws(() => fs.rmdirSync(root), { code: 'EBUSY' });
  assert.throws(() => fs.renameSync(linkA, newPath), erofs);
  assert.throws(() => fs.renameSync(linkA, linkB), erofs);
  assert.throws(() => fs.chmodSync(root, 0o777), erofs);
  assert.throws(() => fs.lchownSync(linkA, 0, 0), erofs);
  assert.throws(() => fs.lutimesSync(linkA, 0, 0), erofs);
  // Only an existing entry is reported as read-only.
  assert.throws(() => fs.unlinkSync(newPath), { code: 'ENOENT' });
  fs.rmSync(newPath, { force: true });
  // Creating what exists fails as it would anywhere.
  assert.throws(() => fs.mkdirSync(root), { code: 'EEXIST' });
  fs.mkdirSync(root, { recursive: true });
  fs.mkdirSync(linkA, { recursive: true });

  // Nothing was removed.
  assert.strictEqual(fs.readlinkSync(linkA), idA);
  assert.strictEqual(a.readFileSync(path.join(mountA, 'dir/file.txt'), 'utf8'), 'from a');

  fs.rm(linkA, common.expectsError(erofs));
  assert.rejects(fs.promises.rm(root, { recursive: true }), erofs).then(common.mustCall());
  assert.rejects(fs.promises.unlink(linkB), erofs).then(common.mustCall());
  assert.rejects(fs.promises.mkdir(newPath), erofs).then(common.mustCall());
}

// A mount point cannot be removed or renamed, nor replaced by a rename.
{
  const busy = { code: 'EBUSY' };
  const c = vfs.create();
  c.mkdirSync('/dir');
  c.writeFileSync('/dir/file.txt', 'data');
  const mountC = c.mount();
  assert.throws(() => fs.rmdirSync(mountC), busy);
  assert.throws(() => fs.renameSync(mountC, path.join(mountC, 'moved')), busy);
  assert.throws(() => fs.renameSync(path.join(mountC, 'dir'), mountC), busy);
  assert.throws(() => c.rmdirSync(mountC), busy);
  // A recursive removal empties the file system, then fails on its root.
  assert.throws(() => fs.rmSync(mountC, { recursive: true }), busy);
  assert.deepStrictEqual(fs.readdirSync(mountC), []);
  assert.throws(() => fs.rmdirSync(mountC), busy);
  assert.ok(fs.statSync(mountC).isDirectory());
  assert.strictEqual(c.mounted, true);

  fs.rmdir(mountC, common.expectsError(busy));
  assert.rejects(fs.promises.rmdir(mountC), busy).then(common.mustCall());
  assert.rejects(fs.promises.rename(mountC, path.join(mountC, 'moved')), busy)
    .then(common.mustCall());
  assert.rejects(fs.promises.rm(mountC, { recursive: true }), busy)
    .then(() => c.unmount()).then(common.mustCall());

  // The same holds for the root of a file system that is not mounted.
  const unmounted = vfs.create();
  assert.throws(() => unmounted.rmdirSync('/'), busy);
  assert.throws(() => unmounted.renameSync('/', '/moved'), busy);
}

// Unmounting removes a layer and the names linking to it; a later mount
// under a name takes the name over.
process.on('beforeExit', common.mustCall(() => {
  const c = vfs.create();
  c.writeFileSync('/file.txt', 'from c');
  const mountC = c.mount('b');
  assert.strictEqual(fs.realpathSync(linkB), mountC);
  assert.strictEqual(fs.readFileSync(path.join(linkB, 'file.txt'), 'utf8'), 'from c');
  // The earlier mount is still there, but no longer under the name.
  assert.strictEqual(b.mounted, true);
  assert.strictEqual(fs.readFileSync(path.join(mountB, 'file.txt'), 'utf8'), 'from b');

  b.unmount();
  assert.strictEqual(fs.realpathSync(linkB), mountC);
  c.unmount();
  assert.throws(() => fs.lstatSync(linkB), { code: 'ENOENT' });
  a.unmount();
  assert.deepStrictEqual(fs.readdirSync(root), [idUnnamed]);

  // With nothing mounted, there is no root directory.
  unnamed.unmount();
  assert.strictEqual(fs.existsSync(root), false);
}));
