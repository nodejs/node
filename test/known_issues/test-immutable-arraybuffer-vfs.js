// Flags: --js-immutable-arraybuffer --experimental-vfs
'use strict';

require('../common');
const assert = require('node:assert');
const { test } = require('node:test');
const vfs = require('node:vfs');
const { ZipEntry, ZipBuffer, createZipArchiveSync } = require('node:zlib');

const data = Buffer.from('test');

function immutable(buffer) {
  return Buffer.from(buffer.buffer.sliceToImmutable(
    buffer.byteOffset, buffer.byteOffset + buffer.length));
}

function createZip() {
  const entry = ZipEntry.createSync('file', data, { method: 'store' });
  const bytes = Buffer.concat([...createZipArchiveSync([entry])]);
  return vfs.create(new vfs.ZipProvider(new ZipBuffer(bytes)));
}

for (const [name, write] of [
  ['writeFileSync', (store, path, source) => store.writeFileSync(path, source)],
  ['promises.writeFile', (store, path, source) => store.promises.writeFile(path, source)],
]) {
  test(`ZipProvider ${name} accepts immutable source bytes`, async () => {
    const store = createZip();
    await write(store, '/control', data);
    assert.deepStrictEqual(store.readFileSync('/control'), data);

    const source = immutable(data);
    await write(store, '/immutable', source);
    assert.deepStrictEqual(store.readFileSync('/immutable'), data);
    assert.deepStrictEqual([...source], [...data]);
  });
}

const reads = [
  ['readSync', (store, fd, buffer) => store.readSync(fd, buffer, 0, buffer.length, null)],
  ['read', (store, fd, buffer) => new Promise((resolve, reject) => {
    store.read(fd, buffer, 0, buffer.length, null, (err, bytesRead) => {
      if (err) reject(err);
      else resolve(bytesRead);
    });
  })],
];

for (const [provider, create] of [
  ['MemoryProvider', () => {
    const store = vfs.create();
    store.writeFileSync('/file', data);
    return store;
  }],
  ['ZipProvider', createZip],
]) {
  for (const [method, read] of reads) {
    test(`${provider} ${method} with an immutable destination`, async (t) => {
      const store = create();
      const fd = store.openSync('/file', 'r');
      t.after(() => store.closeSync(fd));

      const control = Buffer.alloc(data.length);
      assert.strictEqual(store.readSync(fd, control, 0, control.length, 0), data.length);
      assert.deepStrictEqual(control, data);

      const target = immutable(Buffer.alloc(data.length));
      let bytesRead = 0;
      try {
        bytesRead = await read(store, fd, target);
      } catch (err) {
        if (!(err instanceof TypeError)) throw err;
      }
      assert.deepStrictEqual([...target], [0, 0, 0, 0]);
      // Buffer.copy() returns zero for an immutable destination. A read must
      // reject or report no bytes, and must not advance the file position.
      assert.strictEqual(bytesRead, 0);
      control.fill(0);
      assert.strictEqual(store.readSync(fd, control, 0, control.length, null), data.length);
      assert.deepStrictEqual(control, data);
    });
  }
}
