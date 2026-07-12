'use strict';

const common = require('../common');
const {
  spawnSyncAndExitWithoutError,
  spawnSyncAndAssert,
} = require('../common/child_process');

if (process.config.variables.node_without_node_options) {
  common.skip('missing NODE_OPTIONS support');
}

const fixtures = require('../common/fixtures');
spawnSyncAndExitWithoutError(
  process.execPath,
  [
    fixtures.path('spawn-worker-with-copied-env'),
  ],
  {
    env: {
      ...process.env,
      NODE_OPTIONS: '--title=foo'
    }
  }
);

spawnSyncAndAssert(
  process.execPath,
  [
    fixtures.path('spawn-worker-with-trace-exit'),
  ],
  {
    env: {
      ...process.env,
    }
  },
  {
    stderr: /spawn-worker-with-trace-exit\.js:17/
  }
);
