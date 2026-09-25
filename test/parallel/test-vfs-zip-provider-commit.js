// Flags: --experimental-vfs
'use strict';

// A ZipProvider handle commits its content to the archive when it is closed.
// The effects `open(2)` has at open time, and the metadata an entry already
// carries, must survive that model: opening with "w" creates or truncates
// even without a write, rewriting an entry keeps its mode, fstat reports the
// entry's mode, and a file cannot be renamed onto a directory. Each case
// states the real-fs outcome as the expectation. Cases are independent so
// the runner reports each one.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const vfs = require('node:vfs');
const { test } = require('node:test');

// Builds a writable in-memory archive from [name, content, options] triples
// and mounts it, returning the mount point.
function mountZip(entries) {
  const list = entries.map(({ 0: name, 1: content, 2: options }) =>
    zlib.ZipEntry.createSync(name, Buffer.from(content), options));
  const chunks = [];
  for (const chunk of zlib.createZipArchiveSync(list)) chunks.push(chunk);
  const provider = new vfs.ZipProvider(new zlib.ZipBuffer(Buffer.concat(chunks)));
  return vfs.create(provider).mount();
}

test('opening an existing file with "w" truncates it even without a write', () => {
  const file = path.join(mountZip([['f.txt', 'hello']]), 'f.txt');
  fs.closeSync(fs.openSync(file, 'w'));
  assert.strictEqual(fs.readFileSync(file, 'utf8'), '');
});

test('opening a new file with "w" creates it even without a write', () => {
  const file = path.join(mountZip([['f.txt', 'hello']]), 'new.txt');
  fs.closeSync(fs.openSync(file, 'w'));
  assert.strictEqual(fs.existsSync(file), true);
});

test('opening a new file with "a" creates it even without a write', () => {
  const file = path.join(mountZip([['f.txt', 'hello']]), 'log.txt');
  fs.closeSync(fs.openSync(file, 'a'));
  assert.strictEqual(fs.existsSync(file), true);
});

test('appending keeps the entry mode', () => {
  const file = path.join(mountZip([['x.sh', 'a', { mode: 0o755 }]]), 'x.sh');
  assert.strictEqual(fs.statSync(file).mode & 0o777, 0o755);
  fs.appendFileSync(file, 'b');
  assert.strictEqual(fs.statSync(file).mode & 0o777, 0o755);
  assert.strictEqual(fs.readFileSync(file, 'utf8'), 'ab');
});

test('an in-place write keeps the entry mode', () => {
  const file = path.join(mountZip([['x.sh', 'abc', { mode: 0o755 }]]), 'x.sh');
  const fd = fs.openSync(file, 'r+');
  fs.writeSync(fd, Buffer.from('Z'), 0, 1, 0);
  fs.closeSync(fd);
  assert.strictEqual(fs.statSync(file).mode & 0o777, 0o755);
});

test('a new file gets the mode passed to open', () => {
  const file = path.join(mountZip([['f.txt', 'hello']]), 'new.sh');
  const fd = fs.openSync(file, 'w', 0o700);
  fs.writeSync(fd, Buffer.from('#!'));
  fs.closeSync(fd);
  assert.strictEqual(fs.statSync(file).mode & 0o777, 0o700);
});

test('fstat reports the entry mode, not the open() mode argument', () => {
  const file = path.join(mountZip([['x.sh', 'a', { mode: 0o755 }]]), 'x.sh');
  const fd = fs.openSync(file, 'r');
  try {
    assert.strictEqual(fs.fstatSync(fd).mode & 0o777, 0o755);
  } finally {
    fs.closeSync(fd);
  }
});

test('fstat reports the entry modification time', () => {
  const modified = new Date('2020-01-02T03:04:05Z');
  const file = path.join(mountZip([['f.txt', 'a', { modified }]]), 'f.txt');
  const fd = fs.openSync(file, 'r');
  try {
    // ZIP timestamps have two-second resolution, so compare at that grain.
    assert.strictEqual(Math.floor(fs.fstatSync(fd).mtimeMs / 2000),
                       Math.floor(modified.getTime() / 2000));
  } finally {
    fs.closeSync(fd);
  }
});

test('renaming a file onto an existing directory fails with EISDIR', () => {
  const mount = mountZip([['dir/', ''], ['f', 'x']]);
  assert.throws(() => fs.renameSync(path.join(mount, 'f'), path.join(mount, 'dir')),
                { code: 'EISDIR' });
  assert.strictEqual(fs.statSync(path.join(mount, 'dir')).isDirectory(), true);
  assert.strictEqual(fs.readFileSync(path.join(mount, 'f'), 'utf8'), 'x');
});
