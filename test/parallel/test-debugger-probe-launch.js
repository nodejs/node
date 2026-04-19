// This tests that probe launch failures fail fast instead of timing out.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawnSyncAndExit } = require('../common/child_process');
const { probeScript } = require('../common/debugger-probe');

spawnSyncAndExit(process.execPath, [
  'inspect',
  '--probe', `${probeScript}:12`,
  '--expr', 'finalValue',
  '--',
  '--not-a-real-node-flag',
  probeScript,
], {
  signal: null,
  status: 1,
  stderr(output) {
    assert.match(output, /bad option: --not-a-real-node-flag/);
    assert.match(output, /Target exited before the inspector was ready \(code 9\)/);
  },
  trim: true,
});
