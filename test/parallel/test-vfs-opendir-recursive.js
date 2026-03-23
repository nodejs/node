'use strict';

// opendir({ recursive: true }) must return nested entries on mounted VFS.

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/a');
myVfs.writeFileSync('/a/b.txt', 'x');
myVfs.mount('/mnt_or');

const dir = fs.opendirSync('/mnt_or', { recursive: true });
const names = [];
for (;;) {
  const ent = dir.readSync();
  if (!ent) break;
  names.push(ent.name);
}
dir.closeSync();

assert.ok(names.includes('b.txt'),
  `Expected 'b.txt' in entries, got: ${names}`);

myVfs.unmount();
