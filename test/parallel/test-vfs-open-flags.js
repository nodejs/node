'use strict';

// Tests for VFS open flag semantics:
// - Read-only fd rejects writes and ftruncate
// - Write-only fd rejects reads
// - Exclusive flag rejects existing files
// - Numeric flags are correctly normalized

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// Read-only fd (flag 'r') rejects writes.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'abcdef');
  myVfs.mount('/mnt');

  const fd = fs.openSync('/mnt/file.txt', 'r');

  // Writing to a read-only fd must throw EBADF
  assert.throws(
    () => fs.writeSync(fd, Buffer.from('Z'), 0, 1, 0),
    { code: 'EBADF' },
  );

  // Truncating a read-only fd must throw EBADF
  assert.throws(
    () => fs.ftruncateSync(fd, 2),
    { code: 'EBADF' },
  );

  // Content should be unchanged
  assert.strictEqual(fs.readFileSync('/mnt/file.txt', 'utf8'), 'abcdef');

  fs.closeSync(fd);
  myVfs.unmount();
}

// Exclusive flag (wx) rejects existing files with EEXIST.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'orig');
  myVfs.mount('/mnt');

  assert.throws(
    () => fs.openSync('/mnt/file.txt', 'wx'),
    { code: 'EEXIST' },
  );

  myVfs.unmount();
}

// Write-only fd (flag 'w') rejects reads with EBADF.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'abc');
  myVfs.mount('/mnt');

  const fd = fs.openSync('/mnt/file.txt', 'w');
  const buf = Buffer.alloc(10);

  assert.throws(
    () => fs.readSync(fd, buf, 0, 10, 0),
    { code: 'EBADF' },
  );

  fs.closeSync(fd);
  myVfs.unmount();
}

// Numeric flags: O_CREAT | O_TRUNC | O_WRONLY creates a new file like 'w'.
{
  const { O_CREAT, O_TRUNC, O_WRONLY } = fs.constants;
  const myVfs = vfs.create();
  myVfs.mount('/mnt');

  const fd = fs.openSync('/mnt/new.txt', O_CREAT | O_TRUNC | O_WRONLY);
  fs.writeSync(fd, Buffer.from('hello'));
  fs.closeSync(fd);

  assert.strictEqual(fs.readFileSync('/mnt/new.txt', 'utf8'), 'hello');

  myVfs.unmount();
}

// Numeric flags: O_APPEND | O_WRONLY appends to existing file.
{
  const { O_APPEND, O_WRONLY } = fs.constants;
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'abc');
  myVfs.mount('/mnt');

  const fd = fs.openSync('/mnt/file.txt', O_APPEND | O_WRONLY);
  fs.writeSync(fd, Buffer.from('def'));
  fs.closeSync(fd);

  assert.strictEqual(fs.readFileSync('/mnt/file.txt', 'utf8'), 'abcdef');

  myVfs.unmount();
}
