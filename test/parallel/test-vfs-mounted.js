// Flags: --experimental-vfs
'use strict';

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

assert.deepStrictEqual(vfs.mounted(), []);

const a = vfs.create();
const b = vfs.create();
const c = vfs.create();

// Created but not yet mounted.
assert.deepStrictEqual(vfs.mounted(), []);

// Mount order, not creation order.
b.mount();
a.mount();
c.mount();
{
  const list = vfs.mounted();
  assert.strictEqual(list.length, 3);
  assert.strictEqual(list[0], b);
  assert.strictEqual(list[1], a);
  assert.strictEqual(list[2], c);
  assert.ok(list.every((fs) => fs instanceof vfs.VirtualFileSystem));
  assert.ok(list.every((fs) => fs.mounted));
}

// The result is a snapshot; mutating it does not affect the registry.
{
  const list = vfs.mounted();
  list.length = 0;
  assert.strictEqual(vfs.mounted().length, 3);
  assert.notStrictEqual(vfs.mounted(), vfs.mounted());
}

a.unmount();
{
  const list = vfs.mounted();
  assert.strictEqual(list.length, 2);
  assert.strictEqual(list[0], b);
  assert.strictEqual(list[1], c);
}

// Remounting moves the instance to the end.
a.mount();
{
  const list = vfs.mounted();
  assert.strictEqual(list.length, 3);
  assert.strictEqual(list[0], b);
  assert.strictEqual(list[1], c);
  assert.strictEqual(list[2], a);
}

// Unmounting an already-unmounted instance leaves the list unchanged.
b.unmount();
b.unmount();
{
  const list = vfs.mounted();
  assert.strictEqual(list.length, 2);
  assert.strictEqual(list[0], c);
  assert.strictEqual(list[1], a);
}

// Explicit resource management unmounts and removes the entry.
{
  using d = vfs.create();
  d.mount();
  assert.strictEqual(vfs.mounted().at(-1), d);
}
assert.strictEqual(vfs.mounted().length, 2);

a.unmount();
c.unmount();
assert.deepStrictEqual(vfs.mounted(), []);

// Mounts made by --vfs-mount at startup are listed too, in command-line order.
{
  const tmpdir = require('../common/tmpdir');
  const fs = require('fs');
  const path = require('path');
  const { spawnSyncAndAssert } = require('../common/child_process');

  tmpdir.refresh();
  const first = tmpdir.resolve('first');
  const second = tmpdir.resolve('second');
  fs.mkdirSync(first);
  fs.mkdirSync(second);
  fs.writeFileSync(path.join(first, 'name.txt'), 'first');
  fs.writeFileSync(path.join(second, 'name.txt'), 'second');

  spawnSyncAndAssert(process.execPath, [
    '--experimental-vfs',
    `--vfs-mount=${first}`,
    `--vfs-mount=${second}`,
    '-p',
    `const fs = require('fs');
     require('node:vfs').mounted()
       .map((v) => fs.readFileSync(v.mountPoint + '/name.txt', 'utf8'))
       .join(',')`,
  ], {
    stdout: 'first,second',
    trim: true,
  });
}
