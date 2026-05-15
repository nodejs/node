// This tests debugger probe JSON output for duplicate and multi-probe hits.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const probeUrl = fixtures.fileURL('debugger', 'probe.js').href;
const locationAt8 = { url: probeUrl, line: 8, column: 3 };
const locationAt12 = { url: probeUrl, line: 12, column: 1 };

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--json',
  '--probe', 'probe.js:8',
  '--expr', 'index',
  '--probe', 'probe.js:8',
  '--expr', 'total',
  '--probe', 'probe.js:12',
  '--expr', 'finalValue',
  'probe.js',
], { cwd }, {
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes: [
        { expr: 'index', target: { suffix: 'probe.js', line: 8 } },
        { expr: 'total', target: { suffix: 'probe.js', line: 8 } },
        { expr: 'finalValue', target: { suffix: 'probe.js', line: 12 } },
      ],
      results: [
        {
          probe: 0,
          event: 'hit',
          hit: 1,
          location: locationAt8,
          result: { type: 'number', value: 0, description: '0' },
        },
        {
          probe: 1,
          event: 'hit',
          hit: 1,
          location: locationAt8,
          result: { type: 'number', value: 0, description: '0' },
        },
        {
          probe: 0,
          event: 'hit',
          hit: 2,
          location: locationAt8,
          result: { type: 'number', value: 1, description: '1' },
        },
        {
          probe: 1,
          event: 'hit',
          hit: 2,
          location: locationAt8,
          result: { type: 'number', value: 40, description: '40' },
        },
        {
          probe: 2,
          event: 'hit',
          hit: 1,
          location: locationAt12,
          result: { type: 'number', value: 81, description: '81' },
        },
        { event: 'completed' },
      ],
    });
  },
  trim: true,
});
