'use strict';

// Aborting a queued stat must cancel the libuv request instead of running it.
// The threadpool is saturated so the request is still queued when it is
// cancelled; the queue is FIFO, so the stat cannot start before the blocker.

const common = require('../common');

if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

if (process.argv[2] !== 'child') {
  spawnSyncAndExitWithoutError(
    process.execPath,
    ['--expose-internals', __filename, 'child'],
    { env: { ...process.env, UV_THREADPOOL_SIZE: '1' } },
  );
  return;
}

// A single worker is what keeps the stat queued behind the blocker below.
assert.strictEqual(process.env.UV_THREADPOOL_SIZE, '1');

const { internalBinding } = require('internal/test/binding');
const binding = internalBinding('fs');

// Occupy the only worker thread so the stat below cannot start.
crypto.pbkdf2('secret', 'salt', 500_000, 32, 'sha512', common.mustCall());

const req = new binding.FSReqCallback(false);
req.oncomplete = common.mustCall((err) => {
  // Without cancellation the stat would run and report success.
  assert.strictEqual(err?.code, 'ECANCELED');
});
binding.stat(__filename, false, req, true);
req.cancel();
