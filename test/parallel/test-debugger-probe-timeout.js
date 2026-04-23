// This tests probe session timeout behavior and teardown.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const { spawnSyncAndExit } = require('../common/child_process');
const { assertProbeJson, timeoutScript } = require('../common/debugger-probe');

spawnSyncAndExit(process.execPath, [
  'inspect',
  '--json',
  '--timeout=200',
  '--probe', `${timeoutScript}:99`,
  '--expr', '1',
  timeoutScript,
], {
  signal: null,
  status: 1,
  stdout(output) {
    assertProbeJson(output, {
      v: 1,
      probes: [{ expr: '1', target: [timeoutScript, 99] }],
      results: [{
        event: 'timeout',
        pending: [0],
        error: {
          code: 'probe_timeout',
          message: `Timed out after 200ms waiting for probes: ${timeoutScript}:99`,
        },
      }],
    });
  },
  trim: true,
});
