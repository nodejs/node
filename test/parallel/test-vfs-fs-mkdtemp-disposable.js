// Flags: --experimental-vfs
'use strict';

// fs.mkdtempDisposableSync and fs.promises.mkdtempDisposable dispatch to the
// VFS for a mounted prefix, like mkdtempSync and fs.promises.mkdtemp do, and
// their remove() deletes the directory from the VFS. A prefix outside the
// mount still reaches the real file system.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const myVfs = vfs.create();
myVfs.mkdirSync('/work', { recursive: true });
const mountPoint = myVfs.mount();
const prefix = path.join(mountPoint, 'work', 'tmp-');

// Sync: created in the VFS, removed from the VFS, removal is idempotent.
{
  const result = fs.mkdtempDisposableSync(prefix);
  assert.ok(result.path.startsWith(prefix));
  assert.strictEqual(result.path.length, prefix.length + 6);
  assert.strictEqual(fs.statSync(result.path).isDirectory(), true);
  assert.deepStrictEqual(fs.readdirSync(path.join(mountPoint, 'work')), [path.basename(result.path)]);

  fs.writeFileSync(path.join(result.path, 'file.txt'), 'data');
  result.remove();
  assert.strictEqual(fs.existsSync(result.path), false);
  assert.deepStrictEqual(fs.readdirSync(path.join(mountPoint, 'work')), []);
  result.remove();
}

// Sync: Symbol.dispose
{
  const result = fs.mkdtempDisposableSync(prefix);
  assert.strictEqual(fs.existsSync(result.path), true);
  result[Symbol.dispose]();
  assert.strictEqual(fs.existsSync(result.path), false);
  result[Symbol.dispose]();
}

// Sync: buffer encoding
{
  const result = fs.mkdtempDisposableSync(prefix, { encoding: 'buffer' });
  assert.ok(Buffer.isBuffer(result.path));
  assert.ok(result.path.toString().startsWith(prefix));
  assert.strictEqual(fs.existsSync(result.path), true);
  result.remove();
  assert.strictEqual(fs.existsSync(result.path), false);
}

// Sync: a prefix outside the mount uses the real file system.
{
  const result = fs.mkdtempDisposableSync(tmpdir.resolve('real-'));
  assert.strictEqual(path.dirname(result.path), tmpdir.path);
  assert.strictEqual(fs.existsSync(result.path), true);
  result.remove();
  assert.strictEqual(fs.existsSync(result.path), false);
}

(async () => {
  // Promises: created in the VFS, removed from the VFS, removal is idempotent.
  {
    const result = await fs.promises.mkdtempDisposable(prefix);
    assert.ok(result.path.startsWith(prefix));
    assert.strictEqual(result.path.length, prefix.length + 6);
    assert.strictEqual(fs.statSync(result.path).isDirectory(), true);
    assert.deepStrictEqual(fs.readdirSync(path.join(mountPoint, 'work')), [path.basename(result.path)]);

    fs.writeFileSync(path.join(result.path, 'file.txt'), 'data');
    await result.remove();
    assert.strictEqual(fs.existsSync(result.path), false);
    assert.deepStrictEqual(fs.readdirSync(path.join(mountPoint, 'work')), []);
    await result.remove();
  }

  // Promises: Symbol.asyncDispose
  {
    const result = await fs.promises.mkdtempDisposable(prefix);
    assert.strictEqual(fs.existsSync(result.path), true);
    await result[Symbol.asyncDispose]();
    assert.strictEqual(fs.existsSync(result.path), false);
    await result[Symbol.asyncDispose]();
  }

  // Promises: buffer encoding
  {
    const result = await fs.promises.mkdtempDisposable(prefix, { encoding: 'buffer' });
    assert.ok(Buffer.isBuffer(result.path));
    assert.ok(result.path.toString().startsWith(prefix));
    assert.strictEqual(fs.existsSync(result.path), true);
    await result.remove();
    assert.strictEqual(fs.existsSync(result.path), false);
  }

  // Promises: a prefix outside the mount uses the real file system.
  {
    const result = await fs.promises.mkdtempDisposable(tmpdir.resolve('real-'));
    assert.strictEqual(path.dirname(result.path), tmpdir.path);
    assert.strictEqual(fs.existsSync(result.path), true);
    await result.remove();
    assert.strictEqual(fs.existsSync(result.path), false);
  }

  myVfs.unmount();
})().then(common.mustCall());
