// Flags: --experimental-vfs
'use strict';

// MemoryProvider: numeric open flags (mirroring fs.constants.O_*) must be
// normalised to their string equivalents.

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const { O_RDONLY, O_RDWR, O_WRONLY, O_CREAT, O_TRUNC, O_EXCL, O_APPEND } = fs.constants;

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'orig');

// O_RDONLY (0)
myVfs.closeSync(myVfs.openSync('/file.txt', O_RDONLY));

// O_RDWR ('r+')
myVfs.closeSync(myVfs.openSync('/file.txt', O_RDWR));

// 'w' = O_WRONLY | O_CREAT | O_TRUNC
myVfs.closeSync(myVfs.openSync('/created.txt', O_WRONLY | O_CREAT | O_TRUNC));

// 'wx' = O_WRONLY | O_CREAT | O_EXCL
myVfs.closeSync(myVfs.openSync('/excl.txt', O_WRONLY | O_CREAT | O_EXCL));

// 'wx' on an existing file throws EEXIST
assert.throws(
  () => myVfs.openSync('/file.txt', O_WRONLY | O_CREAT | O_EXCL),
  { code: 'EEXIST' });

// 'a' = O_APPEND | O_RDWR | O_CREAT (mapped to 'a+')
myVfs.closeSync(myVfs.openSync('/app.txt', O_APPEND | O_RDWR | O_CREAT));

// 'ax+' = O_APPEND | O_EXCL | O_RDWR | O_CREAT
myVfs.closeSync(myVfs.openSync('/axplus.txt',
                               O_APPEND | O_EXCL | O_RDWR | O_CREAT));

// Bogus non-string non-number defaults to 'r'
myVfs.closeSync(myVfs.openSync('/file.txt', null));
