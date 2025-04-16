'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');

common.skipIfInspectorDisabled();

const {
  Worker,
  isMainThread,
} = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

// Ensures that worker.getHeapStatistics() returns valid data

const assert = require('assert');

if (isMainThread) {
  const name = 'Hello Thread';
  const worker = new Worker(fixtures.path('worker-name.js'), {
    name,
  });
  worker.once('message', common.mustCall(async (message) => {
    const stats = await worker.getHeapStatistics();
    const keys = [
      `total_heap_size`,
      `total_heap_size_executable`,
      `total_physical_size`,
      `total_available_size`,
      `used_heap_size`,
      `heap_size_limit`,
      `malloced_memory`,
      `peak_malloced_memory`,
      `does_zap_garbage`,
      `number_of_native_contexts`,
      `number_of_detached_contexts`,
      `total_global_handles_size`,
      `used_global_handles_size`,
      `external_memory`,
    ].sort();
    assert.deepStrictEqual(keys, Object.keys(stats).sort());
    for (const key of keys) {
      if (key === 'does_zap_garbage') {
        assert.strictEqual(typeof stats[key], 'boolean', `Expected ${key} to be a boolean`);
        continue;
      }
      assert.strictEqual(typeof stats[key], 'number', `Expected ${key} to be a number`);
      assert.ok(stats[key] >= 0, `Expected ${key} to be >= 0`);
    }

    worker.postMessage('done');
  }));

  worker.once('exit', common.mustCall(async (code) => {
    assert.strictEqual(code, 0);
    await assert.rejects(worker.getHeapStatistics(), {
      code: 'ERR_WORKER_NOT_RUNNING'
    });
  }));
}
