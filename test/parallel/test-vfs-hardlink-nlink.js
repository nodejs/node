// Flags: --experimental-vfs
'use strict';

// Test that nlink count is updated correctly when creating/removing hard links.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/src.txt', 'content');

// Initially nlink should be 1
assert.strictEqual(myVfs.statSync('/src.txt').nlink, 1);

// After hard link, nlink should be 2 on both
myVfs.linkSync('/src.txt', '/link.txt');
assert.strictEqual(myVfs.statSync('/src.txt').nlink, 2);
assert.strictEqual(myVfs.statSync('/link.txt').nlink, 2);

// Removing one decrements nlink on the other
myVfs.unlinkSync('/link.txt');
assert.strictEqual(myVfs.statSync('/src.txt').nlink, 1);

// promises.link equivalent
(async () => {
  const v = vfs.create();
  v.writeFileSync('/a', 'x');
  await v.promises.link('/a', '/b');
  assert.strictEqual(v.statSync('/a').nlink, 2);
  assert.strictEqual(v.statSync('/b').nlink, 2);
})().then(common.mustCall());
