// Flags: --experimental-vfs
'use strict';

// fs.glob, fs.globSync and fs.promises.glob walk a mounted VFS: a walk
// whose cwd is under a mount, or whose pattern names one absolutely, reads
// the file system through the fs hooks. A walk of the real file system is
// unchanged while a VFS is mounted.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { promisify } = require('util');
const vfs = require('node:vfs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const myVfs = vfs.create();
const files = ['a.txt', 'sub/b.txt', 'sub/c.md', 'sub/deep/d.txt', '.hidden/e.txt'];
for (const file of files) {
  myVfs.mkdirSync(path.posix.dirname(`/data/${file}`), { recursive: true });
  myVfs.writeFileSync(`/data/${file}`, file);
}
myVfs.symlinkSync('/data/sub', '/data/link');
const mountPoint = myVfs.mount();
const cwd = path.join(mountPoint, 'data');

// The same tree on the real file system
const realDir = tmpdir.resolve('real');
for (const file of files) {
  fs.mkdirSync(path.join(realDir, path.dirname(file)), { recursive: true });
  fs.writeFileSync(path.join(realDir, file), file);
}

const native = (paths) => paths.map((p) => p.replaceAll('/', path.sep)).sort();

async function collect(pattern, options) {
  const results = [];
  for await (const entry of fs.promises.glob(pattern, options)) {
    results.push(entry);
  }
  return results;
}

const apis = [
  ['globSync', async (pattern, options) => fs.globSync(pattern, options)],
  ['glob', promisify(fs.glob)],
  ['promises.glob', collect],
];

async function run(name, glob) {
  // Relative patterns from a cwd under the mount. `**` neither enters
  // dot directories nor follows the symbolic link.
  assert.deepStrictEqual(
    (await glob('**/*.txt', { cwd })).sort(),
    native(['a.txt', 'sub/b.txt', 'sub/deep/d.txt']), name);
  assert.deepStrictEqual(
    (await glob(['*.txt', 'sub/*.md'], { cwd })).sort(),
    native(['a.txt', 'sub/c.md']), name);
  assert.deepStrictEqual(
    (await glob('**/*.txt', { cwd, followSymlinks: true })).sort(),
    native(['a.txt', 'link/b.txt', 'link/deep/d.txt', 'sub/b.txt', 'sub/deep/d.txt']), name);
  assert.deepStrictEqual(
    (await glob('**', { cwd, maxDepth: 1 })).sort(),
    native(['.', 'a.txt', 'link', 'sub']), name);

  // Exclusion by pattern and by callback, with and without Dirents.
  assert.deepStrictEqual(
    (await glob('**/*.txt', { cwd, exclude: ['sub/**'] })).sort(),
    native(['a.txt']), name);
  assert.deepStrictEqual(
    (await glob('**/*.txt', { cwd, exclude: (entry) => entry === 'sub' })).sort(),
    native(['a.txt']), name);
  const dirents = await glob('**/*.txt', {
    cwd,
    withFileTypes: true,
    exclude: common.mustCallAtLeast((dirent) => {
      assert.ok(dirent instanceof fs.Dirent, name);
      return dirent.isDirectory() && dirent.name === 'deep';
    }, 1),
  });
  assert.deepStrictEqual(
    dirents.map((dirent) => path.join(dirent.parentPath, dirent.name)).sort(),
    native(['a.txt', 'sub/b.txt']).map((p) => path.join(cwd, p)), name);
  assert.ok(dirents.every((dirent) => dirent.isFile()), name);

  // An absolute pattern into the mount, from a cwd on the real file
  // system, and a relative one that leaves its directory within the mount.
  assert.deepStrictEqual(
    await glob(path.join(cwd, '**', '*.md'), { cwd: tmpdir.path }),
    [path.join(cwd, 'sub', 'c.md')], name);
  assert.deepStrictEqual(
    await glob('../*.txt', { cwd: path.join(cwd, 'sub') }),
    [path.join('..', 'a.txt')], name);

  // The reserved root lists the mount points, from within and from above.
  assert.ok(
    (await glob('*', { cwd: vfs.vfsBase() })).includes(path.basename(mountPoint)), name);
  assert.ok(
    (await glob(path.join(vfs.vfsBase(), '*'), { cwd: tmpdir.path })).includes(mountPoint), name);
  assert.deepStrictEqual(
    await glob(cwd, { cwd, maxDepth: 0 }), [cwd], name);

  // The real file system, while the VFS is mounted
  assert.deepStrictEqual(
    (await glob('**/*.txt', { cwd: realDir })).sort(),
    native(['a.txt', 'sub/b.txt', 'sub/deep/d.txt']), name);
  assert.deepStrictEqual(
    (await glob(path.join(realDir, '**', '*.md'), { cwd })).sort(),
    [path.join(realDir, 'sub', 'c.md')], name);
}

(async () => {
  for (const [name, glob] of apis) {
    await run(name, glob);
  }

  // An exclude callback that throws fails the walk with its error.
  const error = new Error('exclude failed');
  const throwing = () => { throw error; };
  assert.throws(() => fs.globSync('**', { cwd, exclude: throwing }), (err) => err === error);
  await assert.rejects(promisify(fs.glob)('**', { cwd, exclude: throwing }), (err) => err === error);
  await assert.rejects(collect('**', { cwd, exclude: throwing }), (err) => err === error);

  // Nothing of an async walk runs before the call returns, and leaving
  // the iterator early ends it.
  let calls = 0;
  const exclude = () => { calls++; return false; };
  const iterator = fs.promises.glob('**', { cwd, exclude });
  assert.strictEqual(calls, 0);
  for await (const entry of iterator) {
    assert.strictEqual(typeof entry, 'string');
    break;
  }
  const callsAtBreak = calls;
  await new Promise((resolve) => setTimeout(resolve, 20));
  assert.strictEqual(calls, callsAtBreak);

  // A tree that fills several batches of the iterator
  const manyDir = path.join(mountPoint, 'many');
  fs.mkdirSync(manyDir);
  for (let i = 0; i < 600; i++) {
    fs.writeFileSync(path.join(manyDir, `${i}.txt`), '');
  }
  const many = await collect('*.txt', { cwd: manyDir });
  assert.strictEqual(many.length, 600);

  myVfs.unmount();
  assert.deepStrictEqual(fs.globSync('**', { cwd }), []);
})().then(common.mustCall());
