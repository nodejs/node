'use strict';

// Recursive readdir must follow symlinks to directories.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/real-dir');
myVfs.writeFileSync('/real-dir/nested.txt', 'nested');

myVfs.mkdirSync('/root');
myVfs.symlinkSync('/real-dir', '/root/symdir');

const entries = myVfs.readdirSync('/root', { recursive: true });
assert.ok(entries.includes('symdir'));
assert.ok(
  entries.includes('symdir/nested.txt'),
  `Expected 'symdir/nested.txt' in entries: ${entries}`,
);
