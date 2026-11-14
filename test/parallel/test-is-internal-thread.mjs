import { mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { describe, it } from 'node:test';
import { isInternalThread, Worker } from 'node:worker_threads';

describe('worker_threads.isInternalThread', { concurrency: !process.env.TEST_PARALLEL }, () => {
  it('should be false inside the main thread', async () => {
    assert.strictEqual(isInternalThread, false);
  });

  it('should be false inside a regular worker thread', async () => {
    const worker = new Worker(fixtures.path('worker-is-internal-thread.js'));
    worker.on('message', mustCall((message) => {
      assert.strictEqual(message, 'isInternalThread: false');
    }));
  });
});
