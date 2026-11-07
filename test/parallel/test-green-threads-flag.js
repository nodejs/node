'use strict';

require('../common');
const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');

// The module is only available with --experimental-green-threads.
assert.throws(() => require('node:green_threads'), {
  code: 'ERR_UNKNOWN_BUILTIN_MODULE',
});
assert.throws(() => require('green_threads'), {
  code: 'MODULE_NOT_FOUND',
});

spawnSyncAndAssert(process.execPath, [
  '--experimental-green-threads',
  '-e',
  `
  const assert = require('assert');
  const { GreenThread } = require('node:green_threads');
  assert.throws(() => require('green_threads'), { code: 'MODULE_NOT_FOUND' });
  assert.throws(() => new GreenThread(), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => new GreenThread('', null),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => new GreenThread('', { filename: 1 }),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(
    () => new GreenThread('', { resourceLimits: { maxOldGenerationSizeMb: -1 } }),
    { code: 'ERR_OUT_OF_RANGE' });
  const greenThread = new GreenThread('');
  assert.throws(() => greenThread.evaluate(1), { code: 'ERR_INVALID_ARG_TYPE' });
  greenThread.terminate();
  `,
], {
  stderr: /ExperimentalWarning: Green threads is an experimental feature/,
});
