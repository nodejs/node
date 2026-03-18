'use strict';

// Tests for Buffer encoding and Buffer path arguments with VFS:
// - { encoding: 'buffer' } returns Buffer values from various APIs
// - watch({ encoding: 'buffer' }) emits Buffer filenames
// - Buffer path arguments are intercepted correctly

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// Various fs methods with { encoding: 'buffer' } should return Buffer values.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir');
  myVfs.writeFileSync('/dir/file.txt', 'x');
  myVfs.symlinkSync('/dir/file.txt', '/link');
  myVfs.mount('/mnt');

  // readdirSync with buffer encoding
  const entries = fs.readdirSync('/mnt/dir', { encoding: 'buffer' });
  assert.strictEqual(Buffer.isBuffer(entries[0]), true);

  // readlinkSync with buffer encoding
  const target = fs.readlinkSync('/mnt/link', { encoding: 'buffer' });
  assert.strictEqual(Buffer.isBuffer(target), true);

  // realpathSync with buffer encoding
  const realpath = fs.realpathSync('/mnt/link', { encoding: 'buffer' });
  assert.strictEqual(Buffer.isBuffer(realpath), true);

  // mkdtempSync with buffer encoding
  const tmpdir = fs.mkdtempSync('/mnt/tmp-', { encoding: 'buffer' });
  assert.strictEqual(Buffer.isBuffer(tmpdir), true);

  myVfs.unmount();
}

// fs.watch({ encoding: 'buffer' }) emits Buffer filenames.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'x');

  const watcher = myVfs.watch('/file.txt', {
    interval: 50,
    encoding: 'buffer',
  }, common.mustCall((eventType, filename) => {
    assert.strictEqual(Buffer.isBuffer(filename), true);
    assert.deepStrictEqual(filename, Buffer.from('file.txt'));
    watcher.close();
  }));

  setTimeout(() => myVfs.writeFileSync('/file.txt', 'y'), 100);
}

// Buffer path arguments work with VFS interception.
// Passing a Buffer instead of a string as the path should still be
// intercepted by the VFS handlers.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'ok');
  myVfs.mount('/mnt');

  const result = fs.readFileSync(Buffer.from('/mnt/file.txt'), 'utf8');
  assert.strictEqual(result, 'ok');

  myVfs.unmount();
}
