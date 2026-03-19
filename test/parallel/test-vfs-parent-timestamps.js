// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/dir');
myVfs.mount('/mnt');

function getTimestamps(p) {
  const st = fs.statSync(p);
  return { mtimeMs: st.mtimeMs, ctimeMs: st.ctimeMs };
}

// Creating a file should update parent directory timestamps
{
  const before = getTimestamps('/mnt/dir');
  // Small busy-wait to ensure timestamp changes
  const start = Date.now();
  while (Date.now() - start < 10);
  fs.writeFileSync('/mnt/dir/file.txt', 'hello');
  const after = getTimestamps('/mnt/dir');
  assert.ok(after.mtimeMs > before.mtimeMs,
            `mtime should increase after file creation: ${after.mtimeMs} > ${before.mtimeMs}`);
  assert.ok(after.ctimeMs > before.ctimeMs,
            `ctime should increase after file creation: ${after.ctimeMs} > ${before.ctimeMs}`);
}

// Deleting a file should update parent directory timestamps
{
  const before = getTimestamps('/mnt/dir');
  const start = Date.now();
  while (Date.now() - start < 10);
  fs.unlinkSync('/mnt/dir/file.txt');
  const after = getTimestamps('/mnt/dir');
  assert.ok(after.mtimeMs > before.mtimeMs,
            `mtime should increase after unlink: ${after.mtimeMs} > ${before.mtimeMs}`);
  assert.ok(after.ctimeMs > before.ctimeMs,
            `ctime should increase after unlink: ${after.ctimeMs} > ${before.ctimeMs}`);
}

// Creating a subdirectory should update parent directory timestamps
{
  const before = getTimestamps('/mnt/dir');
  const start = Date.now();
  while (Date.now() - start < 10);
  fs.mkdirSync('/mnt/dir/subdir');
  const after = getTimestamps('/mnt/dir');
  assert.ok(after.mtimeMs > before.mtimeMs,
            `mtime should increase after mkdir: ${after.mtimeMs} > ${before.mtimeMs}`);
}

// Removing a subdirectory should update parent directory timestamps
{
  const before = getTimestamps('/mnt/dir');
  const start = Date.now();
  while (Date.now() - start < 10);
  fs.rmdirSync('/mnt/dir/subdir');
  const after = getTimestamps('/mnt/dir');
  assert.ok(after.mtimeMs > before.mtimeMs,
            `mtime should increase after rmdir: ${after.mtimeMs} > ${before.mtimeMs}`);
}

// Creating a symlink should update parent directory timestamps
{
  fs.writeFileSync('/mnt/dir/target.txt', 'target');
  const before = getTimestamps('/mnt/dir');
  const start = Date.now();
  while (Date.now() - start < 10);
  fs.symlinkSync('/mnt/dir/target.txt', '/mnt/dir/link.txt');
  const after = getTimestamps('/mnt/dir');
  assert.ok(after.mtimeMs > before.mtimeMs,
            `mtime should increase after symlink: ${after.mtimeMs} > ${before.mtimeMs}`);
}

// Creating a hard link should update parent directory timestamps
{
  const before = getTimestamps('/mnt/dir');
  const start = Date.now();
  while (Date.now() - start < 10);
  fs.linkSync('/mnt/dir/target.txt', '/mnt/dir/hardlink.txt');
  const after = getTimestamps('/mnt/dir');
  assert.ok(after.mtimeMs > before.mtimeMs,
            `mtime should increase after link: ${after.mtimeMs} > ${before.mtimeMs}`);
}

// Renaming should update both source and destination parent timestamps
{
  fs.mkdirSync('/mnt/dir/src');
  fs.mkdirSync('/mnt/dir/dst');
  fs.writeFileSync('/mnt/dir/src/file.txt', 'data');
  const beforeSrc = getTimestamps('/mnt/dir/src');
  const beforeDst = getTimestamps('/mnt/dir/dst');
  const start = Date.now();
  while (Date.now() - start < 10);
  fs.renameSync('/mnt/dir/src/file.txt', '/mnt/dir/dst/file.txt');
  const afterSrc = getTimestamps('/mnt/dir/src');
  const afterDst = getTimestamps('/mnt/dir/dst');
  assert.ok(afterSrc.mtimeMs > beforeSrc.mtimeMs,
            `source parent mtime should increase after rename`);
  assert.ok(afterDst.mtimeMs > beforeDst.mtimeMs,
            `dest parent mtime should increase after rename`);
}

myVfs.unmount();
