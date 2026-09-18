// Flags: --experimental-vfs
'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');
const { spawnSyncAndAssert } = require('../common/child_process');

function named(name) {
  return vfs.create(new vfs.MemoryProvider({ name }));
}

// A fresh null-prototype object on every call.
{
  const result = vfs.mounted();
  assert.strictEqual(Object.getPrototypeOf(result), null);
  assert.deepStrictEqual(Object.keys(result), []);
  assert.notStrictEqual(vfs.mounted(), result);
}

// Only mounted file systems are listed, and only while they are mounted.
{
  const a = named('a');
  const b = named('b');
  assert.deepStrictEqual(Object.keys(vfs.mounted()), []);

  a.mount();
  b.mount();
  const result = vfs.mounted();
  assert.deepStrictEqual(Object.keys(result), ['a', 'b']);
  assert.strictEqual(result.a, a);
  assert.strictEqual(result.b, b);

  // Changing the result does not affect the mounts.
  delete result.a;
  assert.strictEqual(vfs.mounted().a, a);

  a.unmount();
  assert.deepStrictEqual(Object.keys(vfs.mounted()), ['b']);
  b.unmount();
  assert.deepStrictEqual(Object.keys(vfs.mounted()), []);
}

// File systems without a name are left out; one named '' is not.
{
  const unnamed = vfs.create();
  const empty = named('');
  unnamed.mount();
  empty.mount();
  const result = vfs.mounted();
  assert.deepStrictEqual(Object.keys(result), ['']);
  assert.strictEqual(result[''], empty);
  unnamed.unmount();
  empty.unmount();
}

// Names are plain keys, even ones that are special on ordinary objects.
{
  const proto = named('__proto__');
  const toString = named('toString');
  proto.mount();
  toString.mount();
  const result = vfs.mounted();
  assert.strictEqual(Object.getPrototypeOf(result), null);
  assert.deepStrictEqual(Object.keys(result), ['__proto__', 'toString']);
  assert.strictEqual(Object.getOwnPropertyDescriptor(result, '__proto__').value,
                     proto);
  assert.strictEqual(result.toString, toString);
  proto.unmount();
  toString.unmount();
}

// With a shared name, the earliest mount wins.
{
  const first = named('dup');
  const second = named('dup');
  second.mount();
  first.mount();
  assert.strictEqual(vfs.mounted().dup, second);

  second.unmount();
  assert.strictEqual(vfs.mounted().dup, first);

  // Remounting makes it the latest mount.
  second.mount();
  assert.strictEqual(vfs.mounted().dup, first);
  first.unmount();
  assert.strictEqual(vfs.mounted().dup, second);
  second.unmount();
}

// Lists file systems named on the command line.
{
  tmpdir.refresh();
  const dir = tmpdir.resolve('assets');
  fs.mkdirSync(dir);
  fs.writeFileSync(path.join(dir, 'data.txt'), 'from the mount');
  fs.writeFileSync(path.join(dir, 'mod.mjs'), 'export default "imported";');

  spawnSyncAndAssert(process.execPath, [
    '--experimental-vfs',
    '--no-warnings',
    `--vfs-mount=${dir}`,
    `--vfs-mount=assets=${dir}`,
    '--input-type=module',
    '-e',
    `import fs from 'node:fs';
     import vfs from 'node:vfs';
     const mounts = vfs.mounted();
     console.log(Object.keys(mounts).join(','));
     const { assets } = mounts;
     console.log(fs.readFileSync(assets.mountPoint + '/data.txt', 'utf8'));
     const mod = await import(assets.mountPointURL + '/mod.mjs');
     console.log(mod.default);`,
  ], {
    stdout: 'assets\nfrom the mount\nimported',
    trim: true,
  });
}
