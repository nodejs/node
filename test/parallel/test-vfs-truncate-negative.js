// Flags: --experimental-vfs
'use strict';

// truncateSync with negative length must clamp to 0, not throw.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'abc');

myVfs.truncateSync('/file.txt', -1);
const content = myVfs.readFileSync('/file.txt', 'utf8');
assert.strictEqual(content, '');
