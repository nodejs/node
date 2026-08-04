// This tests that a --cond expression that throws is treated as false, so the
// probe never records a hit and is reported as missed.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--json',
  '--probe', 'probe-max-hit.js:5',
  '--expr', 'index',
  '--cond', 'index.missing.member',
  'probe-max-hit.js',
], { cwd }, {
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes: [{
        expr: 'index',
        condition: 'index.missing.member',
        target: { suffix: 'probe-max-hit.js', line: 5 },
      }],
      results: [{
        event: 'miss',
        pending: [0],
      }],
    });
  },
  trim: true,
});
