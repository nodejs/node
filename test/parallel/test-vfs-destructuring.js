// Flags: --experimental-vfs
'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const vfs = require('node:vfs');

// Destructure fs methods BEFORE mounting any VFS. Because the guards are
// inside each fs method body (not done via monkey-patching), these captured
// references must still route through VFS once a mount is created.
const {
  readFileSync,
  existsSync,
  statSync,
  lstatSync,
  readdirSync,
  realpathSync,
} = require('fs');

const myVfs = vfs.create();
myVfs.mkdirSync('/sub', { recursive: true });
myVfs.writeFileSync('/file.txt', 'hello from vfs');
myVfs.writeFileSync('/sub/nested.txt', 'nested content');

const MOUNT = myVfs.mount();
const FILE = path.join(MOUNT, 'file.txt');

{
  const content = readFileSync(FILE, 'utf8');
  assert.strictEqual(content, 'hello from vfs');
}

{
  assert.strictEqual(existsSync(FILE), true);
  assert.strictEqual(existsSync(path.join(MOUNT, 'nonexistent')), false);
}

{
  const stats = statSync(FILE);
  assert.strictEqual(stats.isFile(), true);
  assert.strictEqual(stats.isDirectory(), false);
}

{
  const stats = lstatSync(FILE);
  assert.strictEqual(stats.isFile(), true);
}

{
  const entries = readdirSync(MOUNT);
  assert.ok(entries.includes('file.txt'));
  assert.ok(entries.includes('sub'));
}

{
  const real = realpathSync(FILE);
  assert.strictEqual(real, FILE);
}

const { readdir, lstat } = require('fs/promises');

async function testPromises() {
  const entries = await readdir(MOUNT);
  assert.ok(entries.includes('file.txt'));

  const stats = await lstat(FILE);
  assert.strictEqual(stats.isFile(), true);
}

testPromises().then(common.mustCall(() => {
  myVfs.unmount();
}));
