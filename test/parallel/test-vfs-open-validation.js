// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'x');
myVfs.mount('/mnt');

// openSync with invalid flags (object) should throw ERR_INVALID_ARG_VALUE
assert.throws(
  () => fs.openSync('/mnt/file.txt', {}),
  { code: 'ERR_INVALID_ARG_VALUE' },
);

// open (callback) with invalid flags (object) should throw ERR_INVALID_ARG_VALUE
assert.throws(
  () => fs.open('/mnt/file.txt', {}, (err) => {
    assert.fail('callback should not be called');
  }),
  { code: 'ERR_INVALID_ARG_VALUE' },
);

// openSync with invalid flags (boolean) should throw
assert.throws(
  () => fs.openSync('/mnt/file.txt', true),
  { code: 'ERR_INVALID_ARG_VALUE' },
);

// Valid flags should still work
{
  const fd = fs.openSync('/mnt/file.txt', 'r');
  assert.ok((fd & 0x40000000) !== 0);
  fs.closeSync(fd);
}

myVfs.unmount();
