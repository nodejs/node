// Flags: --experimental-vfs
'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const vfs = require('node:vfs');
const { spawnSyncAndAssert } = require('../common/child_process');

const root = path.join(os.devNull, 'vfs');

// A name must be a single path segment that cannot be taken for a layer id.
{
  const myVfs = vfs.create();
  for (const name of ['', '.', '..', '0', '17', '12345', '100000000000000000000',
                      'a/b', 'a\\b', 'a\0b']) {
    assert.throws(() => myVfs.mount(name), { code: 'ERR_INVALID_ARG_VALUE' },
                  JSON.stringify(name));
  }
  for (const name of [null, 1, {}, Symbol('name')]) {
    assert.throws(() => myVfs.mount(name), { code: 'ERR_INVALID_ARG_TYPE' });
  }
  // A rejected name leaves the file system unmounted.
  assert.strictEqual(myVfs.mounted, false);

  // Anything else is a name, including names special on ordinary objects.
  for (const name of ['a', '0a', 'a0', '00', '07', '-0', '-1', '1.5', '1e3', ' 1',
                      '+1', 'NaN', 'Infinity', '-Infinity', '...', '__proto__',
                      'constructor', 'π']) {
    const mountPoint = myVfs.mount(name);
    assert.strictEqual(fs.realpathSync(path.join(root, name)), mountPoint, name);
    assert.strictEqual(fs.readlinkSync(path.join(root, name)),
                       path.basename(mountPoint), name);
    myVfs.unmount();
  }
}

// Mounting a mounted file system under a name fails without taking the name.
{
  const first = vfs.create();
  const second = vfs.create();
  first.mount('taken');
  second.mount();
  assert.throws(() => second.mount('taken'), { code: 'ERR_INVALID_STATE' });
  assert.strictEqual(fs.realpathSync(path.join(root, 'taken')), first.mountPoint);
  first.unmount();
  second.unmount();
}

// Paths through a name reach the layer it links to.
{
  const a = vfs.create();
  a.mkdirSync('/dir');
  a.writeFileSync('/dir/file.txt', 'from a');
  a.writeFileSync('/mod.js', 'module.exports = __filename;');
  a.writeFileSync('/mod.mjs', 'export default import.meta.url;');
  const b = vfs.create();
  b.writeFileSync('/file.txt', 'from b');
  const mountA = a.mount('a');
  const mountB = b.mount('b');
  const linkA = path.join(root, 'a');
  const linkB = path.join(root, 'b');

  assert.strictEqual(fs.realpathSync(linkA), mountA);
  assert.strictEqual(fs.realpathSync.native(linkA), mountA);
  assert.ok(fs.statSync(linkA).isDirectory());
  assert.strictEqual(fs.readFileSync(path.join(linkA, 'dir', 'file.txt'), 'utf8'),
                     'from a');
  assert.deepStrictEqual(fs.readdirSync(linkA).sort(), ['dir', 'mod.js', 'mod.mjs']);
  assert.deepStrictEqual(fs.readdirSync(path.join(linkA, 'dir')), ['file.txt']);
  assert.strictEqual(fs.realpathSync(path.join(linkA, 'dir', 'file.txt')),
                     path.join(mountA, 'dir', 'file.txt'));
  assert.ok(fs.lstatSync(path.join(linkA, 'dir')).isDirectory());
  assert.strictEqual(fs.existsSync(path.join(linkA, 'missing')), false);
  assert.throws(() => fs.readFileSync(path.join(linkA, 'missing')), { code: 'ENOENT' });

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
  // Each layer is a file system of its own, whichever way it is reached.
  assert.throws(() => fs.renameSync(path.join(linkA, 'mod.js'),
                                    path.join(linkB, 'mod.js')),
                { code: 'EXDEV' });

  // In the root, each name is a symbolic link to its layer's id.
  const idA = path.basename(mountA);
  const idB = path.basename(mountB);
  const entries = fs.readdirSync(root, { withFileTypes: true });
  for (const [name, id] of [['a', idA], ['b', idB]]) {
    assert.ok(entries.find((d) => d.name === name)?.isSymbolicLink(), name);
    assert.ok(entries.find((d) => d.name === id)?.isDirectory(), id);
  }
  // A recursive listing does not descend through the links.
  assert.ok(!fs.readdirSync(root, { recursive: true }).some((p) => p.startsWith('a/')));
  assert.ok(fs.lstatSync(linkA).isSymbolicLink());
  assert.strictEqual(fs.readlinkSync(linkA), idA);
  assert.deepStrictEqual(fs.readlinkSync(linkA, { encoding: 'buffer' }), Buffer.from(idA));
  fs.lstat(linkA, common.mustSucceed((stats) => {
    assert.ok(stats.isSymbolicLink());
  }));
  fs.readlink(linkA, common.mustSucceed((target) => {
    assert.strictEqual(target, idA);
  }));
  fs.promises.readlink(linkB).then(common.mustCall((target) => {
    assert.strictEqual(target, idB);
  }));

  // Acting on a name itself, rather than on what it links to, changes the
  // read-only root, never the layer's root directory.
  const erofs = { code: 'EROFS' };
  assert.throws(() => fs.unlinkSync(linkA), erofs);
  assert.throws(() => fs.rmdirSync(linkA), erofs);
  assert.throws(() => fs.rmSync(linkA), erofs);
  assert.throws(() => fs.rmSync(linkA, { recursive: true, force: true }), erofs);
  assert.throws(() => fs.renameSync(linkA, path.join(root, 'moved')), erofs);
  assert.throws(() => fs.renameSync(linkA, linkB), erofs);
  assert.throws(() => fs.lchownSync(linkA, 0, 0), erofs);
  assert.throws(() => fs.lutimesSync(linkA, 0, 0), erofs);
  assert.throws(() => fs.symlinkSync(idA, path.join(root, 'new')), erofs);
  // A name is not a path to follow when it is a prefix.
  assert.throws(() => fs.mkdtempSync(linkA), erofs);
  // Creating what exists succeeds when asked to.
  fs.mkdirSync(linkA, { recursive: true });
  fs.rm(linkA, common.expectsError(erofs));
  assert.rejects(fs.promises.unlink(linkB), erofs).then(common.mustCall());
  assert.strictEqual(fs.readlinkSync(linkA), idA);
  assert.strictEqual(a.readFileSync(path.join(mountA, 'dir', 'file.txt'), 'utf8'),
                     'from a');

  fs.readFile(path.join(linkB, 'file.txt'), 'utf8', common.mustSucceed((data) => {
    assert.strictEqual(data, 'from b');
  }));
  fs.promises.readFile(path.join(linkB, 'file.txt'), 'utf8').then(common.mustCall((data) => {
    assert.strictEqual(data, 'from b');
  }));
  fs.realpath(linkA, common.mustSucceed((real) => {
    assert.strictEqual(real, mountA);
  }));
  fs.promises.realpath(path.join(linkB, 'file.txt')).then(common.mustCall((real) => {
    assert.strictEqual(real, path.join(mountB, 'file.txt'));
  }));

  // The loader resolves a name like any symbolic link.
  assert.strictEqual(require(path.join(linkA, 'mod.js')), path.join(mountA, 'mod.js'));
  assert.strictEqual(require(path.join(linkA, 'mod')), path.join(mountA, 'mod.js'));
  import(pathToFileURL(path.join(linkA, 'mod.mjs')).href).then(common.mustCall((ns) => {
    assert.strictEqual(ns.default, pathToFileURL(path.join(mountA, 'mod.mjs')).href);

    // A later mount under a name takes it over; the earlier file system stays
    // mounted, but only at its own mount point.
    const c = vfs.create();
    c.writeFileSync('/file.txt', 'from c');
    const mountC = c.mount('b');
    assert.strictEqual(fs.realpathSync(linkB), mountC);
    assert.strictEqual(fs.readFileSync(path.join(linkB, 'file.txt'), 'utf8'), 'from c');
    assert.strictEqual(b.mounted, true);
    assert.strictEqual(fs.readFileSync(path.join(mountB, 'file.txt'), 'utf8'), 'from b');

    // Unmounting removes the names linking to the file system, and only them.
    b.unmount();
    assert.strictEqual(fs.realpathSync(linkB), mountC);
    c.unmount();
    assert.strictEqual(fs.existsSync(linkB), false);
    assert.throws(() => fs.lstatSync(linkB), { code: 'ENOENT' });
    assert.strictEqual(fs.realpathSync(linkA), mountA);
    a.unmount();
  }));
}

// A module loaded through a name without resolving it is dropped from the
// caches when the name moves on, so the name then loads from its new target.
spawnSyncAndAssert(process.execPath, [
  '--experimental-vfs', '--no-warnings', '--preserve-symlinks', '-e', `
    const path = require('path');
    const vfs = require('node:vfs');
    const mod = path.join(require('os').devNull, 'vfs', 'lib', 'mod.js');
    const first = vfs.create();
    first.writeFileSync('/mod.js', 'module.exports = "first";');
    first.mount('lib');
    const loaded = [require(mod)];
    const second = vfs.create();
    second.writeFileSync('/mod.js', 'module.exports = "second";');
    second.mount('lib');
    loaded.push(require(mod));
    second.unmount();
    const third = vfs.create();
    third.writeFileSync('/mod.js', 'module.exports = "third";');
    third.mount('lib');
    loaded.push(require(mod));
    console.log(loaded.join());
  `,
], {
  stdout: 'first,second,third',
  trim: true,
});

// --vfs-mount and --vfs-load take an optional `name=` prefix.
{
  tmpdir.refresh();
  const plain = tmpdir.resolve('plain');
  const withEq = tmpdir.resolve('x=y');
  fs.mkdirSync(plain);
  fs.mkdirSync(withEq);
  fs.writeFileSync(path.join(plain, 'id.txt'), 'plain');
  fs.writeFileSync(path.join(withEq, 'id.txt'), 'x=y');
  fs.writeFileSync(path.join(plain, 'index.js'), `
const fs = require('fs');
const path = require('path');
const link = path.join(require('os').devNull, 'vfs', 'app');
console.log(JSON.stringify([process.argv[1], fs.realpathSync(link) === __dirname]));
`);

  // Reports the number of mounts, and the mount each name leads to.
  const report = `
const fs = require('fs');
const path = require('path');
const root = path.join(require('os').devNull, 'vfs');
const names = {};
let layers = 0;
for (const entry of fs.readdirSync(root, { withFileTypes: true })) {
  if (entry.isSymbolicLink()) {
    names[entry.name] = fs.readFileSync(path.join(root, entry.name, 'id.txt'), 'utf8');
  } else {
    layers++;
  }
}
console.log(JSON.stringify([layers, names]));
`;

  function mounts(values, layers, names) {
    spawnSyncAndAssert(process.execPath, [
      '--experimental-vfs', '--no-warnings',
      ...values.map((v) => `--vfs-mount=${v}`),
      '-e', report,
    ], { cwd: tmpdir.path }, {
      stdout(output) {
        assert.deepStrictEqual(JSON.parse(output), [layers, names]);
      },
    });
  }

  mounts([`assets=${plain}`], 1, { assets: 'plain' });
  mounts([plain], 1, {});
  // Only the first `=` separates the name.
  mounts([`a=${withEq}`], 1, { a: 'x=y' });
  // A prefix holding a path separator belongs to the source.
  mounts([withEq], 1, {});
  mounts(['./x=y'], 1, {});
  // An empty name is no name.
  mounts([`=${plain}`], 1, {});
  // Relative sources resolve after the name is removed.
  mounts(['n=plain'], 1, { n: 'plain' });
  mounts([`one=${plain}`, `two=${withEq}`], 2, { one: 'plain', two: 'x=y' });
  // A later mount takes the name over.
  mounts([`dup=${plain}`, `dup=${withEq}`], 2, { dup: 'x=y' });

  // A name the file system cannot be mounted under is an error.
  spawnSyncAndAssert(process.execPath, [
    '--experimental-vfs', '--no-warnings', `--vfs-mount=7=${plain}`, '-e', '',
  ], {
    status: 1,
    stderr: /ERR_INVALID_ARG_VALUE/,
  });

  // --vfs-load names its mount the same way, and process.argv[1] reports
  // the source without the name.
  spawnSyncAndAssert(process.execPath, [
    '--experimental-vfs', '--no-warnings', `--vfs-load=app=${plain}`,
  ], {
    stdout: JSON.stringify([plain, true]),
    trim: true,
  });
}
