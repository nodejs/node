'use strict';
const common = require('../common');
const assert = require('assert');
const v8 = require('v8');
const { Worker, resourceLimits, isMainThread } = require('worker_threads');

if (isMainThread) {
  assert.deepStrictEqual(resourceLimits, {});
}

const testResourceLimits = {
  maxOldGenerationSizeMb: 16,
  maxYoungGenerationSizeMb: 4,
  codeRangeSizeMb: 16,
};

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  const w = new Worker(__filename, { resourceLimits: testResourceLimits });
  assert.deepStrictEqual(w.resourceLimits, testResourceLimits);
  w.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 1);
    assert.deepStrictEqual(w.resourceLimits, {});
  }));
  w.on('error', common.expectsError({
    code: 'ERR_WORKER_OUT_OF_MEMORY',
    message: 'Worker terminated due to reaching memory limit: ' +
    'JS heap out of memory'
  }));
  return;
}

assert.deepStrictEqual(resourceLimits, testResourceLimits);
const array = [];
while (true) {
  // Leave 10 % wiggle room here.
  const usedMB = v8.getHeapStatistics().used_heap_size / 1024 / 1024;
  assert(usedMB < resourceLimits.maxOldGenerationSizeMb * 1.1);

  let seenSpaces = 0;
  for (const { space_name, space_size } of v8.getHeapSpaceStatistics()) {
    if (space_name === 'new_space') {
      seenSpaces++;
      assert(
        space_size / 1024 / 1024 < resourceLimits.maxYoungGenerationSizeMb * 2);
    } else if (space_name === 'old_space') {
      seenSpaces++;
      assert(space_size / 1024 / 1024 < resourceLimits.maxOldGenerationSizeMb);
    } else if (space_name === 'code_space') {
      seenSpaces++;
      assert(space_size / 1024 / 1024 < resourceLimits.codeRangeSizeMb);
    }
  }
  assert.strictEqual(seenSpaces, 3);

  for (let i = 0; i < 100; i++)
    array.push([array]);
}
