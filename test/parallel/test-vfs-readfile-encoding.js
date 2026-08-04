// Flags: --experimental-vfs
'use strict';

// readFileSync with invalid encoding must throw.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'x');

assert.throws(
  () => myVfs.readFileSync('/file.txt', { encoding: 'bogus' }),
  /encoding/i,
);

// Valid encodings should work
assert.strictEqual(myVfs.readFileSync('/file.txt', 'utf8'), 'x');
assert.strictEqual(myVfs.readFileSync('/file.txt', { encoding: 'utf8' }), 'x');
assert.deepStrictEqual(myVfs.readFileSync('/file.txt'), Buffer.from('x'));
