// Flags: --experimental-vfs
'use strict';

// fs.openAsBlob and fs.openAsBlobSync dispatch to VFS and return Blobs over the virtual file.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const nulName = 'nul\0file.txt';
const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello world');
myVfs.writeFileSync(`/src/${nulName}`, 'nul content');
const mountPoint = myVfs.mount();

const filePath = path.join(mountPoint, 'src/hello.txt');
const missingPath = path.join(mountPoint, 'src/missing.txt');
const nulPath = path.join(mountPoint, 'src', nulName);

(async () => {
  try {
    const syncBlob = fs.openAsBlobSync(filePath, { type: 'text/plain' });
    assert.ok(syncBlob instanceof Blob);
    assert.strictEqual(syncBlob.size, 11);
    assert.strictEqual(syncBlob.type, 'text/plain');
    assert.strictEqual(await syncBlob.text(), 'hello world');
    assert.throws(() => fs.openAsBlobSync(missingPath), {
      code: 'ENOENT',
      syscall: 'stat',
      path: missingPath,
    });
    for (const input of [nulPath, Buffer.from(nulPath)]) {
      assert.throws(() => fs.openAsBlobSync(input), {
        code: 'ERR_INVALID_ARG_VALUE',
      });
    }

    const blob = await fs.openAsBlob(filePath);
    assert.ok(blob instanceof Blob);
    assert.strictEqual(blob.size, 11);
    assert.strictEqual(await blob.text(), 'hello world');
  } finally {
    myVfs.unmount();
  }
})().then(common.mustCall());
