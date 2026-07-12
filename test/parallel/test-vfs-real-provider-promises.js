// Flags: --experimental-vfs
'use strict';

// Promises API for RealFSProvider: every async/promises method,
// plus access() existing/missing.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

tmpdir.refresh();
const root = path.join(tmpdir.path, 'real-provider-promises');
fs.mkdirSync(root, { recursive: true });
const myVfs = vfs.create(new vfs.RealFSProvider(root));

(async () => {
  // writeFile + readFile
  await myVfs.promises.writeFile('/a.txt', 'hello');
  assert.strictEqual(await myVfs.promises.readFile('/a.txt', 'utf8'), 'hello');

  // stat / lstat / access
  const st = await myVfs.promises.stat('/a.txt');
  assert.strictEqual(st.size, 5);
  assert.strictEqual((await myVfs.promises.lstat('/a.txt')).isFile(), true);
  await myVfs.promises.access('/a.txt');
  await assert.rejects(myVfs.promises.access('/missing.txt'),
                       { code: 'ENOENT' });

  // mkdir / readdir / rmdir
  await myVfs.promises.mkdir('/d/sub', { recursive: true });
  const entries = await myVfs.promises.readdir('/d');
  assert.deepStrictEqual(entries.sort(), ['sub']);
  await myVfs.promises.rmdir('/d/sub');

  // rename
  await myVfs.promises.writeFile('/old.txt', 'x');
  await myVfs.promises.rename('/old.txt', '/new.txt');
  assert.strictEqual(myVfs.existsSync('/old.txt'), false);
  assert.strictEqual(myVfs.existsSync('/new.txt'), true);

  // unlink
  await myVfs.promises.unlink('/new.txt');
  assert.strictEqual(myVfs.existsSync('/new.txt'), false);

  // copyFile
  await myVfs.promises.copyFile('/a.txt', '/copy.txt');
  assert.strictEqual(await myVfs.promises.readFile('/copy.txt', 'utf8'), 'hello');

  // open async error
  await assert.rejects(myVfs.provider.open('/missing.txt', 'r'),
                       { code: 'ENOENT' });
})().then(common.mustCall());
