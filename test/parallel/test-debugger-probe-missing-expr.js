// This tests that probe mode rejects a --probe without a matching --expr.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const { spawnSyncAndExit } = require('../common/child_process');
const { probeScript } = require('../common/debugger-probe');

spawnSyncAndExit(process.execPath, [
  'inspect',
  '--probe', `${probeScript}:12`,
  probeScript,
], {
  signal: null,
  status: 1,
  stderr: /Each --probe must be followed immediately by --expr/,
  trim: true,
});
