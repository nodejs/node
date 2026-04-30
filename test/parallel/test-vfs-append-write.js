'use strict';

// writeSync in append mode must append, not overwrite.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/append.txt', 'init');

const fd = myVfs.openSync('/append.txt', 'a');
const buf = Buffer.from(' more');
myVfs.writeSync(fd, buf, 0, buf.length);
myVfs.closeSync(fd);

const content = myVfs.readFileSync('/append.txt', 'utf8');
assert.strictEqual(content, 'init more');
