'use strict';

// readFileSync with invalid encoding must throw ERR_INVALID_ARG_VALUE.

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'x');
myVfs.mount('/mnt_re');

// Sync: should throw ERR_INVALID_ARG_VALUE (not ERR_UNKNOWN_ENCODING)
assert.throws(
  () => fs.readFileSync('/mnt_re/file.txt', { encoding: 'bogus' }),
  { code: 'ERR_INVALID_ARG_VALUE' },
);

// Callback: should throw synchronously with ERR_INVALID_ARG_VALUE
assert.throws(
  () => fs.readFile('/mnt_re/file.txt', { encoding: 'bogus' }, () => {}),
  { code: 'ERR_INVALID_ARG_VALUE' },
);

myVfs.unmount();
