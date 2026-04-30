'use strict';

// Verify { bigint: true } returns BigInt values for VFS stats.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'x');

const st = myVfs.statSync('/file.txt', { bigint: true });
assert.strictEqual(typeof st.size, 'bigint');
assert.strictEqual(st.size, 1n);
assert.strictEqual(typeof st.ino, 'bigint');
assert.strictEqual(typeof st.mode, 'bigint');
