'use strict';

// Verify { bigint: true } works for VFS stats.
// statSync with bigint option should return BigInt values for numeric fields.

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'x');
  myVfs.mount('/mnt');

  const st = fs.statSync('/mnt/file.txt', { bigint: true });
  assert.strictEqual(typeof st.size, 'bigint');
  assert.strictEqual(st.size, 1n);

  myVfs.unmount();
}
