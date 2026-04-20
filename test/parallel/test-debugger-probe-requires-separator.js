// This tests that child Node.js flags require -- in probe mode.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const { spawnSyncAndExit } = require('../common/child_process');
const { probeScript } = require('../common/debugger-probe');

spawnSyncAndExit(process.execPath, [
  'inspect',
  '--probe', `${probeScript}:12`,
  '--expr', 'finalValue',
  '--inspect-port=0',
  probeScript,
], {
  signal: null,
  status: 1,
  stderr: /Use -- before child Node\.js flags in probe mode/,
  trim: true,
});
