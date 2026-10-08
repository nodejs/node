// Flags: --experimental-vfs
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const vfs = require('node:vfs');

// The reserved root is where every mount point lives, and vfs.vfsBase() is
// how a program finds it without spelling the path out.
const root = vfs.vfsBase();
assert.strictEqual(root, path.join(os.devNull, 'vfs'));
assert.strictEqual(vfs.vfsBase(), root);

const a = vfs.create();
a.mkdirSync('/dir');
a.writeFileSync('/dir/file.txt', 'from a');
a.writeFileSync('/mod.js', 'module.exports = __filename;');
const b = vfs.create();
b.writeFileSync('/file.txt', 'from b');
const empty = vfs.create();

const mountA = a.mount();
const mountB = b.mount();
const mountEmpty = empty.mount();
assert.strictEqual(path.dirname(mountA), root);
const idA = path.basename(mountA);
const idB = path.basename(mountB);
const idEmpty = path.basename(mountEmpty);
const layerIds = [idA, idB, idEmpty].sort((x, y) => x - y);

// The root lists every layer by id.
{
  assert.deepStrictEqual(fs.readdirSync(root), layerIds);
  assert.deepStrictEqual(fs.readdirSync(`${root}${path.sep}`), layerIds);
  assert.deepStrictEqual(fs.readdirSync(root, { encoding: 'buffer' }),
                         layerIds.map((name) => Buffer.from(name)));
  for (const id of fs.readdirSync(root)) {
    assert.ok([mountA, mountB, mountEmpty].includes(path.join(root, id)), id);
  }

  const dirents = fs.readdirSync(root, { withFileTypes: true });
  assert.deepStrictEqual(dirents.map((d) => d.name), layerIds);
  for (const dirent of dirents) {
    assert.strictEqual(dirent.parentPath, root);
    assert.ok(dirent.isDirectory(), dirent.name);
  }

  const dir = fs.opendirSync(root);
  const names = [];
  let dirent;
  while ((dirent = dir.readSync()) !== null) names.push(dirent.name);
  dir.closeSync();
  assert.deepStrictEqual(names, layerIds);

  fs.readdir(root, common.mustSucceed((result) => {
    assert.deepStrictEqual(result, layerIds);
  }));
  fs.promises.readdir(root).then(common.mustCall((result) => {
    assert.deepStrictEqual(result, layerIds);
  }));
}

// A recursive listing descends into the layers.
{
  const expected = [
    ...layerIds,
    `${idA}/dir`, `${idA}/mod.js`, `${idA}/dir/file.txt`,
    `${idB}/file.txt`,
  ];
  assert.deepStrictEqual(fs.readdirSync(root, { recursive: true }).sort(),
                         expected.sort());

  // Every API names each entry's directory as a host path.
  const options = { recursive: true, withFileTypes: true };
  const check = common.mustCall((dirents) => {
    const file = dirents.find((d) => d.name === 'file.txt' &&
                                     d.parentPath === path.join(mountA, 'dir'));
    assert.ok(file?.isFile());
    const layer = dirents.find((d) => d.name === idA);
    assert.strictEqual(layer?.parentPath, root);
  }, 3);
  check(fs.readdirSync(root, options));
  fs.readdir(root, options, common.mustSucceed(check));
  fs.promises.readdir(root, options).then(common.mustCall((dirents) => {
    check(dirents);
  }));
}

// The root is a directory, and so is each mount point in it.
{
  assert.ok(fs.statSync(root).isDirectory());
  assert.ok(fs.lstatSync(root).isDirectory());
  assert.ok(fs.statSync(root, { bigint: true }).isDirectory());
  assert.ok(fs.existsSync(root));
  fs.accessSync(root, fs.constants.R_OK);
  assert.strictEqual(fs.realpathSync(root), root);
  assert.strictEqual(fs.realpathSync(mountA), mountA);
  assert.throws(() => fs.readlinkSync(root), { code: 'EINVAL' });
  assert.ok(fs.lstatSync(mountA).isDirectory());

  fs.stat(root, common.mustSucceed((stats) => {
    assert.ok(stats.isDirectory());
  }));
  fs.realpath(root, common.mustSucceed((real) => {
    assert.strictEqual(real, root);
  }));
  fs.promises.lstat(root).then(common.mustCall((stats) => {
    assert.ok(stats.isDirectory());
  }));
}

// Anything else under the root does not exist.
{
  for (const missing of ['missing', 'missing/deeper', `${idA}0`, `0${idA}`,
                         `${idA}.js`, '-1']) {
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

// The root is read-only; the file systems in it are not.
{
  const erofs = { code: 'EROFS' };
  const newPath = path.join(root, 'new');
  assert.throws(() => fs.writeFileSync(newPath, ''), erofs);
  assert.throws(() => fs.mkdirSync(newPath), erofs);
  assert.throws(() => fs.mkdirSync(path.join(newPath, 'deeper'), { recursive: true }), erofs);
  assert.throws(() => fs.mkdtempSync(path.join(root, 'tmp-')), erofs);
  assert.throws(() => fs.symlinkSync(idA, newPath), erofs);
  assert.throws(() => fs.openSync(newPath, 'w'), erofs);
  assert.throws(() => fs.openSync(root, 'r+'), erofs);
  assert.throws(() => fs.openSync(root, 'r'), { code: 'EISDIR' });
  assert.throws(() => fs.rmSync(root, { recursive: true, force: true }), erofs);
  // Like the mount points in it, the root cannot be removed.
  assert.throws(() => fs.rmdirSync(root), { code: 'EBUSY' });
  assert.throws(() => fs.chmodSync(root, 0o777), erofs);
  assert.throws(() => fs.utimesSync(root, 0, 0), erofs);
  // Only an existing entry is reported as read-only.
  assert.throws(() => fs.unlinkSync(newPath), { code: 'ENOENT' });
  fs.rmSync(newPath, { force: true });
  // Creating what exists fails as it would anywhere.
  assert.throws(() => fs.mkdirSync(root), { code: 'EEXIST' });
  fs.mkdirSync(root, { recursive: true });
  // A layer and the root are different file systems.
  assert.throws(() => fs.renameSync(path.join(mountA, 'mod.js'), newPath),
                { code: 'EXDEV' });

  // Nothing was removed, and the layers stay writable.
  assert.strictEqual(fs.readFileSync(path.join(mountA, 'dir', 'file.txt'), 'utf8'),
                     'from a');
  fs.writeFileSync(path.join(mountB, 'new.txt'), 'written');
  assert.strictEqual(fs.readFileSync(path.join(mountB, 'new.txt'), 'utf8'), 'written');

  fs.rm(root, { recursive: true }, common.expectsError(erofs));
  assert.rejects(fs.promises.rm(root, { recursive: true }), erofs).then(common.mustCall());
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

// Unmounting removes a layer from the root; with nothing mounted, there is
// no root directory.
process.on('beforeExit', common.mustCall(() => {
  a.unmount();
  b.unmount();
  assert.deepStrictEqual(fs.readdirSync(root), [idEmpty]);
  assert.strictEqual(fs.existsSync(mountA), false);
  empty.unmount();
  assert.strictEqual(fs.existsSync(root), false);
}));
