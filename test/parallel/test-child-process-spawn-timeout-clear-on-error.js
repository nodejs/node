'use strict';

// Measures the child's actual exit time, not just its 'error' event.
// The outer spawnSync timeout catches a leaked inner timer.

const common = require('../common');
const assert = require('node:assert');
const { getEventListeners } = require('node:events');
const { spawn } = require('node:child_process');
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

// A failed spawn never emits 'exit'. The abort listener must be gone by
// the time 'close' fires, or it stays attached to the user's signal.
{
  const controller = new AbortController();
  const cp = spawn(process.execPath, ['--version'], {
    cwd: '/nonexistent/path/that/should/never/exist',
    signal: controller.signal,
  });
  cp.on('error', () => {});
  cp.on('close', common.mustCall(() => {
    assert.strictEqual(getEventListeners(controller.signal, 'abort').length, 0);
  }));
}
