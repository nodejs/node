// This tests debugger probe JSON output for duplicate and multi-probe hits.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeJson, probeScript } = require('../common/debugger-probe');

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--json',
  '--probe', `${probeScript}:8`,
  '--expr', 'index',
  '--probe', `${probeScript}:8`,
  '--expr', 'total',
  '--probe', `${probeScript}:12`,
  '--expr', 'finalValue',
  probeScript,
], {
  stdout(output) {
    assertProbeJson(output, {
      v: 1,
      probes: [
        { expr: 'index', target: [probeScript, 8] },
        { expr: 'total', target: [probeScript, 8] },
        { expr: 'finalValue', target: [probeScript, 12] },
      ],
      results: [
        {
          probe: 0,
          event: 'hit',
          hit: 1,
          result: { type: 'number', value: 0, description: '0' },
        },
        {
          probe: 1,
          event: 'hit',
          hit: 1,
          result: { type: 'number', value: 0, description: '0' },
        },
        {
          probe: 0,
          event: 'hit',
          hit: 2,
          result: { type: 'number', value: 1, description: '1' },
        },
        {
          probe: 1,
          event: 'hit',
          hit: 2,
          result: { type: 'number', value: 40, description: '40' },
        },
        {
          probe: 2,
          event: 'hit',
          hit: 1,
          result: { type: 'number', value: 81, description: '81' },
        },
        { event: 'completed' },
      ],
    });
  },
  trim: true,
});
