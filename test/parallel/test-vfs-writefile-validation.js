// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mount('/mnt');

// writeFileSync with invalid mode should throw ERR_INVALID_ARG_VALUE
assert.throws(
  () => fs.writeFileSync('/mnt/file.txt', 'x', { mode: 'nope' }),
  { code: 'ERR_INVALID_ARG_VALUE' },
);

// writeFileSync with invalid flush should throw ERR_INVALID_ARG_TYPE
assert.throws(
  () => fs.writeFileSync('/mnt/file.txt', 'x', { flush: 'nope' }),
  { code: 'ERR_INVALID_ARG_TYPE' },
);

// writeFile (callback) with invalid mode
assert.throws(
  () => fs.writeFile('/mnt/file.txt', 'x', { mode: 'nope' }, () => {}),
  { code: 'ERR_INVALID_ARG_VALUE' },
);

// writeFile (callback) with invalid flush
assert.throws(
  () => fs.writeFile('/mnt/file.txt', 'x', { flush: 'nope' }, () => {}),
  { code: 'ERR_INVALID_ARG_TYPE' },
);

// appendFileSync with invalid mode
assert.throws(
  () => fs.appendFileSync('/mnt/file.txt', 'x', { mode: 'nope' }),
  { code: 'ERR_INVALID_ARG_VALUE' },
);

// appendFile (callback) with invalid mode
assert.throws(
  () => fs.appendFile('/mnt/file.txt', 'x', { mode: 'nope' }, () => {}),
  { code: 'ERR_INVALID_ARG_VALUE' },
);

// promises.writeFile with invalid mode
assert.rejects(
  fs.promises.writeFile('/mnt/file.txt', 'x', { mode: 'nope' }),
  { code: 'ERR_INVALID_ARG_VALUE' },
).then(() => {
  // promises.writeFile with invalid flush
  return assert.rejects(
    fs.promises.writeFile('/mnt/file.txt', 'x', { flush: 'nope' }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}).then(() => {
  // promises.appendFile with invalid mode
  return assert.rejects(
    fs.promises.appendFile('/mnt/file.txt', 'x', { mode: 'nope' }),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
}).then(() => {
  myVfs.unmount();
});
