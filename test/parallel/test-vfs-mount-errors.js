// Flags: --experimental-vfs --expose-internals
'use strict';

// Error paths in the VFS mount layer:
// - EXDEV when renaming/linking across different VFS instances or VFS<->real
// - last-unmount handler cleanup (vfsState.handlers becomes null again)
// - namespace isolation between layers

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const os = require('os');
const vfs = require('node:vfs');
const { vfsState } = require('internal/fs/utils');

// EXDEV: rename across two different VFS instances
{
  const a = vfs.create();
  const b = vfs.create();
  a.writeFileSync('/file.txt', 'a');
  b.mkdirSync('/x', { recursive: true });
  const mountA = a.mount();
  const mountB = b.mount();

  assert.throws(
    () => fs.renameSync(path.join(mountA, 'file.txt'),
                        path.join(mountB, 'x/file.txt')),
    { code: 'EXDEV' },
  );
  a.unmount();
  b.unmount();
}

// EXDEV: copyFileSync across two different VFS instances
{
  const a = vfs.create();
  const b = vfs.create();
  a.writeFileSync('/file.txt', 'a');
  b.mkdirSync('/x', { recursive: true });
  const mountA = a.mount();
  const mountB = b.mount();

  assert.throws(
    () => fs.copyFileSync(path.join(mountA, 'file.txt'),
                          path.join(mountB, 'x/copy.txt')),
    { code: 'EXDEV' },
  );
  a.unmount();
  b.unmount();
}

// EXDEV: linkSync across two different VFS instances
{
  const a = vfs.create();
  const b = vfs.create();
  a.writeFileSync('/file.txt', 'a');
  b.mkdirSync('/x', { recursive: true });
  const mountA = a.mount();
  const mountB = b.mount();

  assert.throws(
    () => fs.linkSync(path.join(mountA, 'file.txt'),
                      path.join(mountB, 'x/lk.txt')),
    { code: 'EXDEV' },
  );
  a.unmount();
  b.unmount();
}

// EXDEV: rename from VFS to a real-fs path
{
  const a = vfs.create();
  a.writeFileSync('/file.txt', 'a');
  const mountA = a.mount();

  const tmpReal = '/tmp/vfs-mount-real-' + process.pid + '.txt';
  assert.throws(
    () => fs.renameSync(path.join(mountA, 'file.txt'), tmpReal),
    { code: 'EXDEV' },
  );
  a.unmount();
}

// Handler cleanup: after last unmount, vfsState.handlers returns to null
{
  assert.strictEqual(vfsState.handlers, null);
  const x = vfs.create();
  x.mount();
  assert.notStrictEqual(vfsState.handlers, null);
  x.unmount();
  assert.strictEqual(vfsState.handlers, null);

  // And it re-installs on a subsequent mount
  const y = vfs.create();
  y.mount();
  assert.notStrictEqual(vfsState.handlers, null);
  y.unmount();
  assert.strictEqual(vfsState.handlers, null);
}

// Two parallel mounts both register, last-out clears handlers
{
  const a = vfs.create();
  const b = vfs.create();
  a.mount();
  b.mount();
  assert.notStrictEqual(vfsState.handlers, null);
  a.unmount();
  assert.notStrictEqual(vfsState.handlers, null);
  b.unmount();
  assert.strictEqual(vfsState.handlers, null);
}

// Namespace isolation: a path in an unmounted (or never-mounted) layer
// namespace is not served by other active layers.
{
  const a = vfs.create();
  const b = vfs.create();
  a.writeFileSync('/f.txt', 'a');
  b.writeFileSync('/f.txt', 'b');
  const mountA = a.mount();
  const mountB = b.mount();
  assert.notStrictEqual(mountA, mountB);
  b.unmount();
  // B's namespace is dead even though A is still mounted.
  assert.strictEqual(fs.existsSync(path.join(mountB, 'f.txt')), false);
  assert.strictEqual(fs.readFileSync(path.join(mountA, 'f.txt'), 'utf8'), 'a');
  // A malformed layer segment under the VFS root is not served either.
  const bogus = path.join(os.devNull, 'vfs', 'not-a-number', 'f.txt');
  assert.strictEqual(fs.existsSync(bogus), false);
  a.unmount();
}

// Double-mount of same instance rejected
{
  const a = vfs.create();
  a.mount();
  assert.throws(() => a.mount(), { code: 'ERR_INVALID_STATE' });
  a.unmount();
}
