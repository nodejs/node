'use strict';

// Tests that the obsolete --use-largepages option is a no-op. It only prints
// a warning when the value is `on`.

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');

spawnSyncAndAssert(process.execPath, ['--use-largepages=on', '-p', '42'], {
  trim: true,
  stdout: '42',
  stderr: /--use-largepages is no longer supported/,
});

for (const mode of ['off', 'silent']) {
  spawnSyncAndAssert(
    process.execPath,
    [`--use-largepages=${mode}`, '-p', '42'],
    {
      trim: true,
      stdout: '42',
      stderr: '',
    });
}

spawnSyncAndAssert(process.execPath, ['--use-largepages=xyzzy', '-p', '42'], {
  trim: true,
  status: 9,
  stdout: '',
  stderr: /invalid value for --use-largepages/,
});
