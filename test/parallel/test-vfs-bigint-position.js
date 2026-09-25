// Flags: --experimental-vfs
'use strict';

// VFS readSync should accept a BigInt position parameter.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'abcde');

const fd = myVfs.openSync('/file.txt', 'r');
const buf = Buffer.alloc(1);
const bytesRead = myVfs.readSync(fd, buf, 0, 1, 1n);
assert.strictEqual(bytesRead, 1);
assert.strictEqual(buf.toString(), 'b');
myVfs.closeSync(fd);
