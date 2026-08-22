// Flags: --permission --allow-fs-read=*
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const { open } = require('fs/promises');
const fixtures = require('../common/fixtures');

const regularFile = fixtures.path('permission', 'deny', 'regular-file.md');

// FileHandle sync operations must be blocked when the permission model is
// enabled, consistent with fs.fsync() / fs.fsyncSync() and fdatasync variants.
(async () => {
  const fh = await open(regularFile, 'r');
  try {
    await assert.rejects(
      fh.sync(),
      common.expectsError({ code: 'ERR_ACCESS_DENIED' }),
    );
    await assert.rejects(
      fh.datasync(),
      common.expectsError({ code: 'ERR_ACCESS_DENIED' }),
    );
  } finally {
    await fh.close();
  }
})().then(common.mustCall());
