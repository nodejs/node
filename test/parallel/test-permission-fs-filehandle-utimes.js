// Flags: --permission --allow-fs-read=*
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

if (!common.hasCrypto) {
  common.skip('no crypto');
}

const assert = require('assert');
const { open } = require('fs/promises');
const fixtures = require('../common/fixtures');

const regularFile = fixtures.path('permission', 'deny', 'regular-file.md');

// FileHandle.utimes() must be blocked when the permission model is enabled,
// consistent with fs.futimes() / fs.futimesSync().
(async () => {
  const fh = await open(regularFile, 'r');
  try {
    await assert.rejects(
      fh.utimes(Date.now(), Date.now()),
      common.expectsError({ code: 'ERR_ACCESS_DENIED' }),
    );
  } finally {
    await fh.close();
  }
})().then(common.mustCall());
