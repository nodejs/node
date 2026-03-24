'use strict';

// truncateSync with negative length must clamp to 0, not throw.

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'abc');
myVfs.mount('/mnt_tn');

fs.truncateSync('/mnt_tn/file.txt', -1);
const content = fs.readFileSync('/mnt_tn/file.txt', 'utf8');
assert.strictEqual(content, '');

myVfs.unmount();
