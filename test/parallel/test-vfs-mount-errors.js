// Flags: --experimental-vfs --expose-internals
'use strict';

// Error paths in the VFS mount layer:
// - EXDEV when renaming/linking across different VFS instances or VFS<->real
// - lastunmount handler cleanup (vfsState.handlers becomes null again)
// - rename of root mount point is rejected as overlapping

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');
const { vfsState } = require('internal/fs/utils');

const baseMountPoint = path.resolve('/tmp/vfs-mount-errors-' + process.pid);
let mountCounter = 0;
const nextMount = () => baseMountPoint + '-' + (mountCounter++);

// EXDEV: rename across two different VFS instances
{
  const mountA = nextMount();
  const mountB = nextMount();
  const a = vfs.create();
  const b = vfs.create();
  a.writeFileSync('/file.txt', 'a');
  b.mkdirSync('/x', { recursive: true });
  a.mount(mountA);
  b.mount(mountB);

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
  const mountA = nextMount();
  const mountB = nextMount();
  const a = vfs.create();
  const b = vfs.create();
  a.writeFileSync('/file.txt', 'a');
  b.mkdirSync('/x', { recursive: true });
  a.mount(mountA);
  b.mount(mountB);

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
  const mountA = nextMount();
  const mountB = nextMount();
  const a = vfs.create();
  const b = vfs.create();
  a.writeFileSync('/file.txt', 'a');
  b.mkdirSync('/x', { recursive: true });
  a.mount(mountA);
  b.mount(mountB);

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
  const mountA = nextMount();
  const a = vfs.create();
  a.writeFileSync('/file.txt', 'a');
  a.mount(mountA);

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
  x.mount(nextMount());
  assert.notStrictEqual(vfsState.handlers, null);
  x.unmount();
  assert.strictEqual(vfsState.handlers, null);

  // And it re-installs on a subsequent mount
  const y = vfs.create();
  y.mount(nextMount());
  assert.notStrictEqual(vfsState.handlers, null);
  y.unmount();
  assert.strictEqual(vfsState.handlers, null);
}

// Two parallel non-overlapping mounts both register, last-out clears handlers
{
  const a = vfs.create();
  const b = vfs.create();
  a.mount(nextMount());
  b.mount(nextMount());
  assert.notStrictEqual(vfsState.handlers, null);
  a.unmount();
  assert.notStrictEqual(vfsState.handlers, null);
  b.unmount();
  assert.strictEqual(vfsState.handlers, null);
}

// Overlap detection: nested-under and parent-of both rejected
{
  const parent = nextMount();
  const child = path.join(parent, 'child');
  const a = vfs.create();
  const b = vfs.create();
  a.mount(parent);
  assert.throws(() => b.mount(child), { code: 'ERR_INVALID_STATE' });
  a.unmount();

  // Reverse direction: child first, then parent rejected
  const c = vfs.create();
  const d = vfs.create();
  c.mount(child);
  assert.throws(() => d.mount(parent), { code: 'ERR_INVALID_STATE' });
  c.unmount();
}

// Equal mount points: second one rejected
{
  const m = nextMount();
  const a = vfs.create();
  const b = vfs.create();
  a.mount(m);
  assert.throws(() => b.mount(m), { code: 'ERR_INVALID_STATE' });
  a.unmount();
}

// Double-mount of same instance rejected
{
  const a = vfs.create();
  a.mount(nextMount());
  assert.throws(() => a.mount(nextMount()), { code: 'ERR_INVALID_STATE' });
  a.unmount();
}
