'use strict';

// Exercises the native recursive readdir (sync, callback and promise forms)
// against a reference walk built from the non-recursive API.

const common = require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const root = tmpdir.resolve('tree');
fs.mkdirSync(path.join(root, 'a', 'b', 'c'), { recursive: true });
fs.mkdirSync(path.join(root, 'd'));
fs.mkdirSync(path.join(root, '.hidden'));
fs.mkdirSync(path.join(root, 'empty'));
fs.writeFileSync(path.join(root, 'top'), '');
fs.writeFileSync(path.join(root, 'a', '1'), '');
fs.writeFileSync(path.join(root, 'a', 'b', '2'), '');
fs.writeFileSync(path.join(root, 'a', 'b', 'c', '3'), '');
fs.writeFileSync(path.join(root, 'd', '4'), '');
fs.writeFileSync(path.join(root, '.hidden', '5'), '');

const canSymlink = common.canCreateSymLink();
if (canSymlink) {
  fs.symlinkSync(path.join(root, 'a'), path.join(root, 'd', 'link-to-a'), 'dir');
  fs.symlinkSync(path.join(root, 'a', '1'), path.join(root, 'd', 'link-to-file'));
  fs.symlinkSync(path.join(root, 'nowhere'), path.join(root, 'd', 'dangling'));
}

// Reference implementation: breadth-first, entries in readdir order,
// directory symlinks followed (fs.readdir semantics).
function reference(basePath) {
  const entries = [];
  const queue = [basePath];
  for (let i = 0; i < queue.length; i++) {
    const dir = queue[i];
    for (const dirent of fs.readdirSync(dir, { withFileTypes: true })) {
      const full = path.join(dir, dirent.name);
      entries.push({
        relative: path.relative(basePath, full),
        isDirent: true,
        name: dirent.name,
        parentPath: dir,
        isDirectory: dirent.isDirectory(),
        isSymbolicLink: dirent.isSymbolicLink(),
        isFile: dirent.isFile(),
      });
      let isDir = dirent.isDirectory();
      if (!isDir && dirent.isSymbolicLink()) {
        try {
          isDir = fs.statSync(full).isDirectory();
        } catch {
          isDir = false;
        }
      }
      if (isDir) queue.push(full);
    }
  }
  return entries;
}

function fromDirents(dirents) {
  return dirents.map((dirent) => {
    return {
      isDirent: dirent instanceof fs.Dirent,
      name: dirent.name,
      parentPath: dirent.parentPath,
      isDirectory: dirent.isDirectory(),
      isSymbolicLink: dirent.isSymbolicLink(),
      isFile: dirent.isFile(),
    };
  });
}

function stripRelative(entries) {
  return entries.map(({ relative, ...rest }) => rest);
}

const expected = reference(root);
const expectedPaths = expected.map((entry) => entry.relative);
const expectedDirents = stripRelative(expected);

// The tree is only interesting if symlinks were followed.
if (canSymlink) {
  assert(expectedPaths.includes(path.join('d', 'link-to-a', 'b', 'c', '3')));
  assert(!expectedPaths.includes(path.join('d', 'link-to-file', '1')));
}
assert(expectedPaths.includes(path.join('.hidden', '5')));
assert(expectedPaths.includes('empty'));

// Sync.
assert.deepStrictEqual(fs.readdirSync(root, { recursive: true }), expectedPaths);
assert.deepStrictEqual(
  fromDirents(fs.readdirSync(root, { recursive: true, withFileTypes: true })),
  expectedDirents,
);

// Callback.
fs.readdir(root, { recursive: true }, common.mustSucceed((paths) => {
  assert.deepStrictEqual(paths, expectedPaths);
}));
fs.readdir(root, { recursive: true, withFileTypes: true }, common.mustSucceed((dirents) => {
  assert.deepStrictEqual(fromDirents(dirents), expectedDirents);
}));

// Promises.
(async () => {
  assert.deepStrictEqual(
    await fs.promises.readdir(root, { recursive: true }),
    expectedPaths,
  );
  assert.deepStrictEqual(
    fromDirents(await fs.promises.readdir(root, { recursive: true, withFileTypes: true })),
    expectedDirents,
  );
})().then(common.mustCall());

// Mutating the options object after the call must not affect the result.
{
  const options = { recursive: true, withFileTypes: true };
  fs.readdir(root, options, common.mustSucceed((dirents) => {
    assert.deepStrictEqual(fromDirents(dirents), expectedDirents);
  }));
  options.withFileTypes = false;
  options.recursive = false;
}

// Relative paths are relative to the path as given, while parentPath is the
// path as given for the top level and a joined path below it.
{
  const relativeRoot = path.relative(process.cwd(), root) + path.sep;
  const paths = fs.readdirSync(relativeRoot, { recursive: true });
  assert.deepStrictEqual(paths, expectedPaths);
  const dirents = fs.readdirSync(relativeRoot, { recursive: true, withFileTypes: true });
  assert.strictEqual(dirents[0].parentPath, relativeRoot);
  const nested = dirents.find((dirent) => dirent.name === '1');
  assert.strictEqual(nested.parentPath, path.join(relativeRoot, 'a'));
}

// Buffer encodings and Buffer paths.
{
  const buffers = fs.readdirSync(root, { recursive: true, encoding: 'buffer' });
  assert(buffers.every((entry) => Buffer.isBuffer(entry)));
  assert.deepStrictEqual(buffers.map(String), expectedPaths);

  const stringify = (dirent) => ({
    ...fromDirents([dirent])[0],
    name: String(dirent.name),
    parentPath: String(dirent.parentPath),
  });
  const dirents = fs.readdirSync(root, { recursive: true, encoding: 'buffer', withFileTypes: true });
  assert(dirents.every((dirent) => Buffer.isBuffer(dirent.name)));
  assert(dirents.every((dirent) => dirent.parentPath === root || Buffer.isBuffer(dirent.parentPath)));
  assert.deepStrictEqual(dirents.map(stringify), expectedDirents);

  // A Buffer path keeps parentPath a Buffer, whatever the encoding.
  const bufferRoot = Buffer.from(root);
  assert.deepStrictEqual(fs.readdirSync(bufferRoot, { recursive: true }), expectedPaths);
  const bufferDirents = fs.readdirSync(bufferRoot, { recursive: true, withFileTypes: true });
  assert(bufferDirents.every((dirent) => typeof dirent.name === 'string'));
  assert(bufferDirents.every((dirent) => Buffer.isBuffer(dirent.parentPath)));
  assert.strictEqual(bufferDirents[0].parentPath, bufferRoot);
  assert.deepStrictEqual(bufferDirents.map(stringify), expectedDirents);

  fs.readdir(bufferRoot, { recursive: true }, common.mustSucceed((paths) => {
    assert.deepStrictEqual(paths, expectedPaths);
  }));
  fs.readdir(bufferRoot, { recursive: true, withFileTypes: true, encoding: 'buffer' }, common.mustSucceed((dirents) => {
    assert(dirents.every((dirent) => Buffer.isBuffer(dirent.name) && Buffer.isBuffer(dirent.parentPath)));
    assert.deepStrictEqual(dirents.map(stringify), expectedDirents);
  }));
  fs.promises.readdir(bufferRoot, { recursive: true, encoding: 'buffer' }).then(common.mustCall((paths) => {
    assert.deepStrictEqual(paths.map(String), expectedPaths);
  }));
  fs.promises.readdir(bufferRoot, { recursive: true, withFileTypes: true }).then(common.mustCall((dirents) => {
    assert(dirents.every((dirent) => Buffer.isBuffer(dirent.parentPath)));
    assert.deepStrictEqual(dirents.map(stringify), expectedDirents);
  }));
}

// Errors carry the directory that failed, for the root and below it.
{
  const missing = path.join(root, 'missing');
  assert.throws(() => fs.readdirSync(missing, { recursive: true }), {
    code: 'ENOENT',
    syscall: 'scandir',
    path: missing,
  });
  const file = path.join(root, 'top');
  assert.throws(() => fs.readdirSync(file, { recursive: true }), {
    code: 'ENOTDIR',
    syscall: 'scandir',
    path: file,
  });
  fs.readdir(missing, { recursive: true }, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'scandir');
    assert.strictEqual(err.path, missing);
  }));
  assert.rejects(fs.promises.readdir(file, { recursive: true }), {
    code: 'ENOTDIR',
    syscall: 'scandir',
    path: file,
  }).then(common.mustCall());
}

if (!common.isWindows && !common.isIBMi && process.getuid() !== 0) {
  const locked = tmpdir.resolve('locked');
  const inner = path.join(locked, 'inner');
  fs.mkdirSync(inner, { recursive: true });
  fs.chmodSync(inner, 0);
  try {
    assert.throws(() => fs.readdirSync(locked, { recursive: true }), {
      code: 'EACCES',
      syscall: 'scandir',
      path: inner,
    });
    fs.readdir(locked, { recursive: true }, common.mustCall((err) => {
      assert.strictEqual(err.code, 'EACCES');
      assert.strictEqual(err.path, inner);
      fs.chmodSync(inner, 0o755);
    }));
  } catch (err) {
    fs.chmodSync(inner, 0o755);
    throw err;
  }
}
