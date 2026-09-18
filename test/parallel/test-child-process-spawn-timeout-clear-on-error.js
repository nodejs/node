'use strict';

// Measures the child's actual exit time, not just its 'error' event.
// The outer spawnSync timeout catches a leaked inner timer.

const common = require('../common');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

const bugStallMs = common.platformTimeout(10000);
const outerTimeoutMs = common.platformTimeout(2000);

spawnSyncAndExitWithoutError(process.execPath, ['-e', `
  const { spawn } = require('child_process');
  const cp = spawn(process.execPath, ['--version'], {
    cwd: '/nonexistent/path/that/should/never/exist',
    timeout: ${bugStallMs},
  });
  cp.on('error', () => {});
`], { timeout: outerTimeoutMs });
