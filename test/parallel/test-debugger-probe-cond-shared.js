// This tests that two probes at the same location with the same --cond share a
// single breakpoint and both evaluate only when the condition holds.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');
const { assertProbeJson } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');
const probeUrl = fixtures.fileURL('debugger', 'probe-max-hit.js').href;

spawnSyncAndAssert(process.execPath, [
  'inspect',
  '--json',
  '--probe', 'probe-max-hit.js:5',
  '--expr', 'index',
  '--cond', 'index === 1',
  '--probe', 'probe-max-hit.js:5',
  '--expr', 'total',
  '--cond', 'index === 1',
  'probe-max-hit.js',
], { cwd }, {
  stdout(output) {
    assertProbeJson(output, {
      v: 2,
      probes: [
        {
          expr: 'index',
          condition: 'index === 1',
          target: { suffix: 'probe-max-hit.js', line: 5 },
        },
        {
          expr: 'total',
          condition: 'index === 1',
          target: { suffix: 'probe-max-hit.js', line: 5 },
        },
      ],
      results: [
        {
          probe: 0,
          event: 'hit',
          hit: 1,
          location: { url: probeUrl, line: 5, column: 3 },
          result: { type: 'number', value: 1, description: '1' },
        },
        {
          probe: 1,
          event: 'hit',
          hit: 1,
          location: { url: probeUrl, line: 5, column: 3 },
          result: { type: 'number', value: 1, description: '1' },
        },
        { event: 'completed' },
      ],
    });
  },
  trim: true,
});
