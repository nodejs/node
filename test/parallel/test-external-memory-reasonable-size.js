'use strict';

// Node disables V8's external memory reasonable size check by default, but
// explicit values on the command line must still be honored.
// Refs: https://github.com/nodejs/node/issues/65534

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');

// Despite the "default" label, --v8-options prints the parsed flag values.
// Inspect them without allocating over a gigabyte and crashing the child.
for (const [flags, expected] of [
  [[], 0],
  [['--external-memory-max-reasonable-size=1'], 1],
  [['--external_memory_max_reasonable_size=1'], 1],
]) {
  spawnSyncAndAssert(process.execPath, [...flags, '--v8-options'], {
    stdout: new RegExp(`default: --external-memory-max-reasonable-size=${expected}\\r?$`, 'm'),
  });
}
