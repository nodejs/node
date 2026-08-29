'use strict';

// V8 aborts the process when external memory grows by more than
// --external-memory-max-reasonable-size gigabytes in a single step. Node
// disables that check by default, but an explicit value on the command line
// must still be honored.
// Refs: https://github.com/nodejs/node/issues/65534

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const { totalmem } = require('os');

// The smallest limit V8 accepts is 1 GB, so the child has to allocate more
// than that before the check can fire.
if (totalmem() < 4 * 1024 ** 3)
  common.skip('not enough memory to exceed a 1 GB external memory limit');

for (const flag of [
  '--external-memory-max-reasonable-size=1',
  '--external_memory_max_reasonable_size=1',
]) {
  const child = spawnSync(process.execPath, [
    flag, '-e', 'new Float64Array(150_000_000)',
  ]);

  assert.notStrictEqual(
    child.status,
    0,
    `${flag} was not honored, the child exited cleanly`,
  );
  assert.match(child.stderr.toString(), /kMaxReasonableBytes/);
}
