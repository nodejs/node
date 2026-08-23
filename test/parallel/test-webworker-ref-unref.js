// Flags: --experimental-web-worker
'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');

for (const method of ['ref', 'unref']) {
  assert.throws(() => Worker.prototype[method].call({}),
                { code: 'ERR_INVALID_THIS' });
}

{
  // Both are no-ops on a worker whose script could not be loaded.
  const worker = new Worker(fixtures.fileURL('web-worker', 'nonexistent.js').href);
  worker.addEventListener('error', common.mustCall());
  worker.unref();
  worker.ref();
}

const worker = new Worker(fixtures.fileURL('web-worker', 'echo.js').href);

worker.addEventListener('error', common.mustNotCall('worker failed'));
worker.addEventListener('message', common.mustCall(({ data }) => {
  assert.strictEqual(data, 'hello');
  worker.terminate();
}));

process.once('beforeExit', common.mustCall(() => {
  worker.ref();
  worker.postMessage('hello');
}));

worker.unref();
