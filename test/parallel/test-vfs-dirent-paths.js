'use strict';

// readdir({ withFileTypes: true }) Dirents must have correct mounted parentPath.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/a');
myVfs.writeFileSync('/a/b.txt', 'x');
myVfs.mount('/mnt_dp');

// Non-recursive: parentPath should be mount path
const entries = fs.readdirSync('/mnt_dp', { withFileTypes: true });
const first = entries.find((e) => e.name === 'a');
assert.ok(first, 'Expected entry "a"');
assert.strictEqual(first.parentPath, '/mnt_dp');

// Recursive: name should be basename, parentPath should be correct parent
const recursive = fs.readdirSync('/mnt_dp', {
  recursive: true,
  withFileTypes: true,
});
const bEntry = recursive.find((e) => e.name === 'b.txt');
assert.ok(bEntry, `Expected "b.txt", got: ${recursive.map((e) => e.name)}`);
assert.strictEqual(bEntry.parentPath, path.join('/mnt_dp', 'a'));

myVfs.unmount();
