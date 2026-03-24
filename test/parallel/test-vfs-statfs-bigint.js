// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'x');
myVfs.mount('/mnt');

// statfsSync with bigint: true should return BigInt values
{
  const st = fs.statfsSync('/mnt/file.txt', { bigint: true });
  assert.strictEqual(typeof st.type, 'bigint', `type should be bigint, got ${typeof st.type}`);
  assert.strictEqual(typeof st.bsize, 'bigint', `bsize should be bigint, got ${typeof st.bsize}`);
  assert.strictEqual(typeof st.blocks, 'bigint');
  assert.strictEqual(typeof st.bfree, 'bigint');
  assert.strictEqual(typeof st.bavail, 'bigint');
  assert.strictEqual(typeof st.files, 'bigint');
  assert.strictEqual(typeof st.ffree, 'bigint');
}

// statfsSync without bigint should return number values
{
  const st = fs.statfsSync('/mnt/file.txt');
  assert.strictEqual(typeof st.type, 'number');
  assert.strictEqual(typeof st.bsize, 'number');
}

// statfs (callback) with bigint: true
{
  fs.statfs('/mnt/file.txt', { bigint: true }, (err, st) => {
    assert.ifError(err);
    assert.strictEqual(typeof st.type, 'bigint');
    assert.strictEqual(typeof st.bsize, 'bigint');

    // promises.statfs with bigint: true
    fs.promises.statfs('/mnt/file.txt', { bigint: true }).then((st2) => {
      assert.strictEqual(typeof st2.type, 'bigint');
      assert.strictEqual(typeof st2.bsize, 'bigint');
      myVfs.unmount();
    });
  });
}
