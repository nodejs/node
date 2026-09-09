// Flags: --experimental-vfs
'use strict';

// fs.statSync / fs.lstatSync / fs.statfsSync dispatch through the VFS layer,
// including the `throwIfNoEntry: false` option.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello world');
const mountPoint = myVfs.mount();

// statSync on a regular file
{
  const s = fs.statSync(path.join(mountPoint, 'src/hello.txt'));
  assert.strictEqual(s.isFile(), true);
  assert.strictEqual(s.size, 11);
}

// statSync with throwIfNoEntry:false on missing path
assert.strictEqual(
  fs.statSync(path.join(mountPoint, 'missing'), { throwIfNoEntry: false }),
  undefined,
);

// statSync on missing path throws ENOENT by default
assert.throws(() => fs.statSync(path.join(mountPoint, 'missing')),
              { code: 'ENOENT' });

// lstatSync on a regular file
{
  const s = fs.lstatSync(path.join(mountPoint, 'src/hello.txt'));
  assert.strictEqual(s.isFile(), true);
}

// lstatSync with throwIfNoEntry:false on missing path
assert.strictEqual(
  fs.lstatSync(path.join(mountPoint, 'missing'), { throwIfNoEntry: false }),
  undefined,
);

// statfsSync returns number-typed values by default
{
  const s = fs.statfsSync(path.join(mountPoint, 'src/hello.txt'));
  assert.strictEqual(typeof s.bsize, 'number');
}

// statfsSync with bigint:true returns BigInt fields
{
  const s = fs.statfsSync(path.join(mountPoint, 'src/hello.txt'),
                          { bigint: true });
  assert.strictEqual(typeof s.bsize, 'bigint');
}

// statfsSync on a missing path throws ENOENT
assert.throws(() => fs.statfsSync(path.join(mountPoint, 'missing')),
              {
                code: 'ENOENT',
                syscall: 'statfs',
                path: path.join(mountPoint, 'missing'),
              });

// Statfs on a missing path reports ENOENT through the callback
fs.statfs(path.join(mountPoint, 'missing'), common.mustCall((err) => {
  assert.strictEqual(err.code, 'ENOENT');
  assert.strictEqual(err.syscall, 'statfs');
  assert.strictEqual(err.path, path.join(mountPoint, 'missing'));
}));

myVfs.unmount();
