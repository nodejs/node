// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const { FileHandle } = require('internal/fs/promises');
const { FileHandle: NativeFileHandle } = internalBinding('fs');

// Test that closing a FileHandle with an invalid fd fails with EBADF.
// Use a high invalid fd so background I/O cannot reuse it before close runs.

(async function() {
  const fh = common.runWithInvalidFD(
    (fd) => new FileHandle(new NativeFileHandle(fd)),
  );

  await assert.rejects(() => fh.close(), {
    code: 'EBADF',
    syscall: 'close'
  });
})().then(common.mustCall());
