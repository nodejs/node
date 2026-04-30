'use strict';

// VFS stats must have non-zero, unique ino and a distinctive dev.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/f1.txt', 'a');
myVfs.writeFileSync('/f2.txt', 'b');

const s1 = myVfs.statSync('/f1.txt');
const s2 = myVfs.statSync('/f2.txt');

// dev should be distinctive VFS value (4085 = 0xVF5)
assert.strictEqual(s1.dev, 4085);
assert.strictEqual(s2.dev, 4085);

// ino should be unique per call
assert.notStrictEqual(s1.ino, 0);
assert.notStrictEqual(s2.ino, 0);
assert.notStrictEqual(s1.ino, s2.ino);
