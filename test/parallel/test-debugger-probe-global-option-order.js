// This tests that global probe options can appear after the first --probe.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeJson, probeScript } = require('../common/debugger-probe');

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--probe', `${probeScript}:12`,
  '--expr', 'finalValue',
  '--json',
  probeScript,
], {
  stdout(output) {
    assertProbeJson(output, {
      v: 1,
      probes: [{
        expr: 'finalValue',
        target: [probeScript, 12],
      }],
      results: [{
        probe: 0,
        event: 'hit',
        hit: 1,
        result: { type: 'number', value: 81, description: '81' },
      }, {
        event: 'completed',
      }],
    });
  },
  trim: true,
});
