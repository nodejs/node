'use strict';
const assert = require('assert');
const v8 = require('v8');

function allocateUntilCrash(resourceLimits) {
  const array = [];
  while (true) {
    const usedMB = v8.getHeapStatistics().used_heap_size / 1024 / 1024;
    const maxReservedSize = resourceLimits.maxOldGenerationSizeMb +
                            resourceLimits.maxYoungGenerationSizeMb;
    assert(usedMB < maxReservedSize);

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
}

module.exports = { allocateUntilCrash };
