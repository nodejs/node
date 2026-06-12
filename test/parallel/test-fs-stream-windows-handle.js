'use strict';

// Tests option validation for the `windowsHandle` option of
// fs.createReadStream()/createWriteStream(). The functional round-trip on
// Windows (where a real Win32 HANDLE is wrapped in a file descriptor) is
// covered by test/addons/fs-windows-handle.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const handle = 1n;

for (const create of [fs.createReadStream, fs.createWriteStream]) {
  assert.throws(() => create(null, { windowsHandle: handle, fd: 2 }), {
    code: 'ERR_INCOMPATIBLE_OPTION_PAIR',
  });
}

if (!common.isWindows) {
  for (const create of [fs.createReadStream, fs.createWriteStream]) {
    assert.throws(() => create(null, { windowsHandle: handle }), {
      code: 'ERR_FEATURE_UNAVAILABLE_ON_PLATFORM',
    });
  }
  return;
}

for (const create of [fs.createReadStream, fs.createWriteStream]) {
  // Cannot be combined with a custom `fs` implementation.
  assert.throws(() => create(null, { windowsHandle: handle, fs: {} }), {
    code: 'ERR_METHOD_NOT_IMPLEMENTED',
  });

  // Must be a bigint.
  assert.throws(() => create(null, { windowsHandle: 'nope' }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => create(null, { windowsHandle: 1 }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  // Must fit into 64 bits.
  assert.throws(() => create(null, { windowsHandle: 2n ** 64n }), {
    code: 'ERR_OUT_OF_RANGE',
  });
}
