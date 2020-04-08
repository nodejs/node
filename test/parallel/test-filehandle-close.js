'use strict';
const common = require('../common');
const assert = require('assert');
const { closeSync, promises: { open } } = require('fs');

// Test that using FileHandle.close to close an already-closed fd fails
// with EBADF.

(async function() {
  const fh = await open(__filename);
  closeSync(fh.fd);

  assert.rejects(() => fh.close(), {
    code: 'EBADF',
    syscall: 'close'
  });
})().then(common.mustCall());
