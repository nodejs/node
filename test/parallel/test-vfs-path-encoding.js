// Flags: --experimental-vfs
'use strict';

// readlink(), realpath() and mkdtemp() on a mounted VFS apply the encoding
// option the way fs does: given as a string or in an object, 'buffer' or any
// other encoding, and a Buffer mkdtemp() prefix always gives a Buffer. The
// VirtualFileSystem readlink and realpath methods do the same.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { promisify } = require('util');
const vfs = require('node:vfs');
const tmpdir = require('../common/tmpdir');

function decode(value, encoding) {
  if (encoding === 'buffer') {
    assert.ok(Buffer.isBuffer(value));
    return value.toString();
  }
  assert.strictEqual(typeof value, 'string');
  return Buffer.from(value, encoding).toString();
}

const memoryVfs = vfs.create();
memoryVfs.writeFileSync('/file.txt', 'x');
memoryVfs.symlinkSync('file.txt', '/link');
memoryVfs.symlinkSync('/file.txt', '/abs-link');
const vfsInstances = [memoryVfs];

if (common.canCreateSymLink()) {
  tmpdir.refresh();
  fs.writeFileSync(tmpdir.resolve('file.txt'), 'x');
  fs.symlinkSync('file.txt', tmpdir.resolve('link'));
  // RealFSProvider returns an absolute target inside its root as a VFS path.
  fs.symlinkSync(tmpdir.resolve('file.txt'), tmpdir.resolve('abs-link'));
  vfsInstances.push(vfs.create(new vfs.RealFSProvider(tmpdir.path)));
}

const readlinks = [
  fs.readlinkSync,
  promisify(fs.readlink),
  fs.promises.readlink,
];
const realpaths = [
  fs.realpathSync,
  fs.realpathSync.native,
  promisify(fs.realpath),
  promisify(fs.realpath.native),
  fs.promises.realpath,
];
const mkdtemps = [
  fs.mkdtempSync,
  promisify(fs.mkdtemp),
  fs.promises.mkdtemp,
];

(async () => {
  for (const myVfs of vfsInstances) {
    const mountPoint = myVfs.mount();
    const prefix = path.join(mountPoint, 'tmp-');
    const vfsReadlinks = [
      myVfs.readlinkSync.bind(myVfs),
      promisify(myVfs.readlink.bind(myVfs)),
      myVfs.promises.readlink,
    ];
    const vfsRealpaths = [
      myVfs.realpathSync.bind(myVfs),
      promisify(myVfs.realpath.bind(myVfs)),
      myVfs.promises.realpath,
    ];

    for (const options of ['buffer', { encoding: 'buffer' }, 'hex', { encoding: 'hex' }]) {
      const encoding = options.encoding ?? options;

      for (const name of ['link', 'abs-link']) {
        const link = path.join(mountPoint, name);
        for (const readlink of [...readlinks, ...vfsReadlinks]) {
          assert.strictEqual(decode(await readlink(link, options), encoding),
                             fs.readlinkSync(link));
        }
        for (const realpath of [...realpaths, ...vfsRealpaths]) {
          assert.strictEqual(decode(await realpath(link, options), encoding),
                             fs.realpathSync(link));
        }
      }

      for (const mkdtemp of mkdtemps) {
        const dir = decode(await mkdtemp(prefix, options), encoding);
        assert.ok(dir.startsWith(prefix));
        assert.ok(fs.statSync(dir).isDirectory());
      }
    }

    for (const mkdtemp of mkdtemps) {
      const dir = await mkdtemp(Buffer.from(prefix), 'hex');
      assert.ok(Buffer.isBuffer(dir));
      assert.ok(fs.statSync(dir).isDirectory());
    }

    myVfs.unmount();
  }
})().then(common.mustCall());
