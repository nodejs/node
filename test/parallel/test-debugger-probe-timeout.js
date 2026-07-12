// This tests probe session timeout behavior and teardown.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndExit } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');
const cwd = fixtures.path('debugger');

const timeout = common.platformTimeout(1000);
spawnSyncAndExit(process.execPath, [
  'inspect',
  '--json',
  `--timeout=${timeout}`,
  '--probe', 'probe-timeout.js:99',
  '--expr', '1',
  'probe-timeout.js',
], { cwd, env: { ...process.env, NODE_DEBUG: 'inspect_probe' } }, {
  signal: null,
  status: 1,
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes: [{
        expr: '1',
        target: { suffix: 'probe-timeout.js', line: 99 },
      }],
      results: [{
        event: 'timeout',
        pending: [0],
        error: {
          code: 'probe_timeout',
          message: `Timed out after ${timeout}ms waiting for probes: probe-timeout.js:99`,
        },
      }],
    });
  },
  trim: true,
});
