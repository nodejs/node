// Flags: --experimental-vfs
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

// VirtualFileSystem API
{
  const myVfs = vfs.create();
  const first = myVfs.mkstempSync('/tmp-');
  const second = myVfs.mkstempSync('/tmp-');
  assert.notStrictEqual(first.path, second.path);
  assert.ok(first.path.startsWith('/tmp-'));
  assert.strictEqual(first.path.length, '/tmp-'.length + 6);
  assert.ok(myVfs.statSync(first.path).isFile());
  myVfs.writeSync(first.fd, Buffer.from('hello'), 0, 5, 0);
  myVfs.closeSync(first.fd);
  myVfs.closeSync(second.fd);
  assert.strictEqual(myVfs.readFileSync(first.path, 'utf8'), 'hello');

  myVfs.mkstemp('/tmp-', common.mustSucceed(({ path, fd }) => {
    assert.ok(path.startsWith('/tmp-'));
    myVfs.closeSync(fd);
  }));
  myVfs.mkstemp('/tmp-', {}, common.mustSucceed(({ fd }) => myVfs.closeSync(fd)));
  myVfs.mkstemp('/missing/prefix-', common.expectsError({ code: 'ENOENT' }));

  myVfs.promises.mkstemp('/tmp-').then(common.mustCall(({ path, fd }) => {
    assert.ok(path.startsWith('/tmp-'));
    myVfs.closeSync(fd);
  }));
  assert.rejects(myVfs.promises.mkstemp('/missing/prefix-'), { code: 'ENOENT' })
    .then(common.mustCall());

  // Test disposal on VirtualFileSystem
  {
    const file = myVfs.mkstempSync('/tmp-disp-');
    assert.strictEqual(typeof file.remove, 'function');
    assert.strictEqual(typeof file[Symbol.dispose], 'function');
    assert.ok(myVfs.statSync(file.path).isFile());
    file.remove();
    assert.throws(() => myVfs.statSync(file.path), { code: 'ENOENT' });
    file.remove(); // idempotent
  }
}

// node:fs on a mounted path
{
  const layer = vfs.create();
  layer.mkdirSync('/d');
  const dir = path.join(layer.mount(), 'd');
  const prefix = path.join(dir, 'f-');

  function check(file) {
    assert.strictEqual(path.dirname(file), dir);
    assert.strictEqual(path.basename(file).length, 'f-'.length + 6);
    assert.strictEqual(fs.readFileSync(file, 'utf8'), 'hello');
  }

  {
    const { path: file, fd } = fs.mkstempSync(prefix);
    fs.writeSync(fd, 'hello');
    fs.closeSync(fd);
    check(file);
  }

  for (const args of [[prefix, 'buffer'], [prefix, { encoding: 'buffer' }], [Buffer.from(prefix)]]) {
    const { path: file, fd } = fs.mkstempSync(...args);
    assert.ok(Buffer.isBuffer(file));
    fs.writeSync(fd, 'hello');
    fs.closeSync(fd);
    check(file.toString());
  }

  fs.mkstemp(prefix, common.mustSucceed(({ path: file, fd }) => {
    fs.writeSync(fd, 'hello');
    fs.closeSync(fd);
    check(file);
  }));

  fs.mkstemp(prefix, 'buffer', common.mustSucceed(({ path: file, fd }) => {
    assert.ok(Buffer.isBuffer(file));
    fs.closeSync(fd);
  }));

  (async () => {
    const { path: file, handle } = await fs.promises.mkstemp(prefix);
    await handle.writeFile('hello');
    await handle.close();
    check(file);

    const result = await fs.promises.mkstemp(Buffer.from(prefix));
    assert.ok(Buffer.isBuffer(result.path));
    await result.handle.close();

    const disposable = await fs.promises.mkstemp(prefix);
    assert.strictEqual(typeof disposable.remove, 'function');
    assert.strictEqual(typeof disposable[Symbol.asyncDispose], 'function');
    assert.ok(fs.existsSync(disposable.path));
    await disposable.remove();
    assert.ok(!fs.existsSync(disposable.path));
    await disposable.remove(); // idempotent
  })().then(common.mustCall());

  {
    const file = fs.mkstempSync(prefix);
    assert.strictEqual(typeof file.remove, 'function');
    assert.strictEqual(typeof file[Symbol.dispose], 'function');
    assert.ok(fs.existsSync(file.path));
    file.remove();
    assert.ok(!fs.existsSync(file.path));
    file.remove(); // idempotent
  }

  {
    const file = fs.mkstempSync(prefix);
    file[Symbol.dispose]();
    assert.ok(!fs.existsSync(file.path));
  }

  const missing = path.join(dir, 'missing', 'f-');
  assert.throws(() => fs.mkstempSync(missing), { code: 'ENOENT' });
  fs.mkstemp(missing, common.expectsError({ code: 'ENOENT' }));
  assert.rejects(fs.promises.mkstemp(missing), { code: 'ENOENT' }).then(common.mustCall());
}
