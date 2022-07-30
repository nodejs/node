'use strict';

require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const fixture = fixtures.path('thread-max-old-space-size.js');
const { spawnSync } = require('child_process');
const resourceLimits = {
  maxOldGenerationSizeMb: 16,
  maxYoungGenerationSizeMb: 4,
  // Set codeRangeSizeMb really high to effectively ignore it.
  codeRangeSizeMb: 999999,
  stackSizeMb: 4,
};
const res = spawnSync(process.execPath, [
  `--stack-size=${1024 * resourceLimits.stackSizeMb}`,
  `--thread-max-old-space-size=${resourceLimits.maxOldGenerationSizeMb}`,
  `--max-semi-space-size=${resourceLimits.maxYoungGenerationSizeMb / 2}`,
  fixture,
  JSON.stringify(resourceLimits),
]);
assert(res.stderr.toString('utf8').includes('Allocation failed - JavaScript heap out of memory'));
